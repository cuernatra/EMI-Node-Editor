#include "inspectorPanel.h"

#include "ui/theme.h"

#include "editor/graph/graphState.h"
#include "editor/graph/graphSerializer.h"
#include "editor/graph/graphEditorUtils.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ed = ax::NodeEditor;

namespace
{
struct InspectorStructFieldDef
{
	std::string name;
	std::string type;
};

static std::string TrimCopy(const std::string& s)
{
	size_t a = 0;
	size_t b = s.size();
	while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
	while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
	return s.substr(a, b - a);
}

static std::vector<std::string> SplitTopLevelArrayItems(const std::string& text)
{
	std::string src = TrimCopy(text);
	if (src.size() >= 2 && src.front() == '[' && src.back() == ']')
		src = src.substr(1, src.size() - 2);

	std::vector<std::string> out;
	std::string current;
	int bracketDepth = 0;
	bool inQuote = false;
	char quoteChar = '\0';
	bool escape = false;

	auto pushCurrent = [&]()
	{
		std::string item = TrimCopy(current);
		if (!item.empty())
			out.push_back(item);
		current.clear();
	};

	for (char ch : src)
	{
		if (escape)
		{
			current.push_back(ch);
			escape = false;
			continue;
		}

		if (inQuote)
		{
			current.push_back(ch);
			if (ch == '\\')
				escape = true;
			else if (ch == quoteChar)
				inQuote = false;
			continue;
		}

		if (ch == '"' || ch == '\'')
		{
			inQuote = true;
			quoteChar = ch;
			current.push_back(ch);
			continue;
		}

		if (ch == '[') ++bracketDepth;
		else if (ch == ']') bracketDepth = std::max(0, bracketDepth - 1);

		if (ch == ',' && bracketDepth == 0)
		{
			pushCurrent();
			continue;
		}

		current.push_back(ch);
	}

	pushCurrent();
	return out;
}

static std::vector<InspectorStructFieldDef> ParseStructDefs(const std::string& raw)
{
	std::vector<InspectorStructFieldDef> defs;
	const std::vector<std::string> items = SplitTopLevelArrayItems(raw);
	for (std::string item : items)
	{
		item = TrimCopy(item);
		if (item.size() >= 2 && ((item.front() == '"' && item.back() == '"') || (item.front() == '\'' && item.back() == '\'')))
			item = item.substr(1, item.size() - 2);

		const size_t sep = item.find(':');
		if (sep == std::string::npos)
			continue;

		InspectorStructFieldDef def;
		def.name = TrimCopy(item.substr(0, sep));
		def.type = TrimCopy(item.substr(sep + 1));
		if (!def.name.empty())
			defs.push_back(def);
	}
	return defs;
}

static std::string BuildStructDefs(const std::vector<InspectorStructFieldDef>& defs)
{
	std::string out = "[";
	for (size_t i = 0; i < defs.size(); ++i)
	{
		if (i != 0)
			out += ", ";
		out += "\"" + defs[i].name + ":" + defs[i].type + "\"";
	}
	out += "]";
	return out;
}

static std::vector<std::string> CollectDefinedStructNames(const GraphState& state)
{
	std::vector<std::string> names;
	for (const auto& node : state.GetNodes())
	{
		if (!node.alive || node.nodeType != NodeType::StructDefine)
			continue;

		const NodeField* nameField = GraphEditorUtils::FindField(node.fields, "Struct Name");
		const std::string name = (nameField && !nameField->value.empty()) ? nameField->value : "test";
		if (std::find(names.begin(), names.end(), name) == names.end())
			names.push_back(name);
	}
	return names;
}
}

void InspectorPanel::draw(GraphState& state, uintptr_t selectedNodeRawId)
{
	ImGui::TextColored(colors::accent, "INSPECTOR PANEL");
	ImGui::Separator();

	if (selectedNodeRawId == 0)
		return;

	const ed::NodeId selectedNodeId(selectedNodeRawId);
	VisualNode* selectedNode = nullptr;
	for (auto& node : state.GetNodes())
	{
		if (!node.alive)
			continue;

		if (node.id == selectedNodeId)
		{
			selectedNode = &node;
			break;
		}
	}

	if (!selectedNode)
		return;

	const ImVec2 nodePos = ed::GetNodePosition(selectedNode->id);

	ImGui::TextColored(colors::textSecondary, "Node ID: %llu", static_cast<unsigned long long>(selectedNode->id.Get()));
	ImGui::TextColored(colors::textSecondary, "Type: %s", NodeTypeToString(selectedNode->nodeType));
	ImGui::TextColored(colors::textSecondary, "Position: (%.1f, %.1f)", nodePos.x, nodePos.y);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(colors::accent, "VALUES");
	ImGui::Spacing();

	bool fieldsChanged = false;

	auto isInputPinConnectedByName = [&](const char* pinName) -> bool
	{
		if (!selectedNode || !pinName)
			return false;

		Pin* targetPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, pinName);
		if (!targetPin || targetPin->type == PinType::Flow)
			return false;

		for (const Link& l : state.GetLinks())
		{
			if (l.alive && l.endPinId == targetPin->id)
				return true;
		}
		return false;
	};

	if (selectedNode->fields.empty())
	{
		ImGui::TextColored(colors::textSecondary, "No values.");
	}
	else
	{
		if (selectedNode->nodeType == NodeType::Variable)
		{
			NodeField* variantField = GraphEditorUtils::FindField(selectedNode->fields, "Variant");
			NodeField* nameField = GraphEditorUtils::FindField(selectedNode->fields, "Name");
			NodeField* typeField = GraphEditorUtils::FindField(selectedNode->fields, "Type");

			const bool isGet = variantField && variantField->value == "Get";
			bool defaultConnected = false;
			if (!isGet)
			{
				Pin* defaultPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, "Default");
				if (!defaultPin)
					defaultPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, "Set");

				if (defaultPin)
				{
					for (const Link& l : state.GetLinks())
					{
						if (l.alive && l.endPinId == defaultPin->id)
						{
							defaultConnected = true;
							break;
						}
					}
				}
			}

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

			if (isGet)
			{
				std::vector<std::string> setVariableNames;

				for (const auto& node : state.GetNodes())
				{
					if (!node.alive || node.nodeType != NodeType::Variable)
						continue;

					const NodeField* nVariant = GraphEditorUtils::FindField(node.fields, "Variant");
					if (!nVariant || nVariant->value != "Set")
						continue;

					const NodeField* nName = GraphEditorUtils::FindField(node.fields, "Name");
					const std::string varName = nName ? nName->value : "myVar";

					if (std::find(setVariableNames.begin(), setVariableNames.end(), varName) == setVariableNames.end())
						setVariableNames.push_back(varName);
				}

				if (nameField)
				{
					if (setVariableNames.empty())
					{
						if (!nameField->value.empty())
						{
							nameField->value.clear();
							fieldsChanged = true;
						}
						ImGui::TextUnformatted("Variable");
						ImGui::SameLine();
						ImGui::TextDisabled("(no Set variables)");
					}
					else
					{
						if (std::find(setVariableNames.begin(), setVariableNames.end(), nameField->value) == setVariableNames.end())
						{
							nameField->value = setVariableNames.front();
							fieldsChanged = true;
						}

						if (ImGui::BeginCombo("Variable", nameField->value.c_str()))
						{
							for (const auto& varName : setVariableNames)
							{
								const bool selected = (nameField->value == varName);
								if (ImGui::Selectable(varName.c_str(), selected))
								{
									nameField->value = varName;
									fieldsChanged = true;
								}
								if (selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
				}

				if (typeField)
				{
					GraphEditorUtils::DrawInspectorReadOnlyField(*typeField);
				}
			}
			else
			{
				for (NodeField& field : selectedNode->fields)
				{
					if (&field == variantField)
						continue;

					if ((field.name == "Default" && defaultConnected)
						|| isInputPinConnectedByName(field.name.c_str()))
					{
						GraphEditorUtils::DrawInspectorReadOnlyField(field);
						continue;
					}

					fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
				}
			}

			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::ForEach
			  || selectedNode->nodeType == NodeType::ArrayGetAt
			  || selectedNode->nodeType == NodeType::ArrayAddAt
			  || selectedNode->nodeType == NodeType::ArrayRemoveAt)
		{
			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
			for (NodeField& field : selectedNode->fields)
			{
				if (isInputPinConnectedByName(field.name.c_str()))
				{
					GraphEditorUtils::DrawInspectorReadOnlyField(field);
					continue;
				}
				fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
			}
			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::StructDefine)
		{
			NodeField* nameField = GraphEditorUtils::FindField(selectedNode->fields, "Struct Name");
			NodeField* defsField = GraphEditorUtils::FindField(selectedNode->fields, "Fields");

			std::vector<InspectorStructFieldDef> defs = ParseStructDefs(defsField ? defsField->value : "[]");

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

			if (nameField)
				fieldsChanged |= GraphEditorUtils::DrawInspectorField(*nameField);

			ImGui::Spacing();
			ImGui::TextColored(colors::accent, "STRUCT FIELDS");

			int removeIndex = -1;
			const char* typeItems[] = { "Number", "Boolean", "String", "Array", "Any" };

			for (int i = 0; i < static_cast<int>(defs.size()); ++i)
			{
				ImGui::PushID(i);

				char nameBuf[128] = {};
				std::snprintf(nameBuf, sizeof(nameBuf), "%s", defs[static_cast<size_t>(i)].name.c_str());
				ImGui::SetNextItemWidth(180.0f);
				if (ImGui::InputText("##fieldName", nameBuf, sizeof(nameBuf)))
				{
					defs[static_cast<size_t>(i)].name = TrimCopy(nameBuf);
					fieldsChanged = true;
				}

				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::BeginCombo("##fieldType", defs[static_cast<size_t>(i)].type.c_str()))
				{
					for (const char* item : typeItems)
					{
						const bool selected = (defs[static_cast<size_t>(i)].type == item);
						if (ImGui::Selectable(item, selected))
						{
							defs[static_cast<size_t>(i)].type = item;
							fieldsChanged = true;
						}
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				if (ImGui::SmallButton("Remove") && removeIndex < 0)
					removeIndex = i;

				ImGui::PopID();
			}

			if (removeIndex >= 0)
			{
				defs.erase(defs.begin() + removeIndex);
				fieldsChanged = true;
			}

			if (ImGui::Button("Add Field"))
			{
				defs.push_back({ "field", "Number" });
				fieldsChanged = true;
			}

			if (defsField && fieldsChanged)
				defsField->value = BuildStructDefs(defs);

			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::StructCreate)
		{
			NodeField* structNameField = GraphEditorUtils::FindField(selectedNode->fields, "Struct Name");
			const std::vector<std::string> structNames = CollectDefinedStructNames(state);

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

			if (structNameField)
			{
				if (!structNames.empty())
				{
					if (std::find(structNames.begin(), structNames.end(), structNameField->value) == structNames.end())
					{
						structNameField->value = structNames.front();
						fieldsChanged = true;
					}

					if (ImGui::BeginCombo("Struct", structNameField->value.c_str()))
					{
						for (const std::string& name : structNames)
						{
							const bool selected = (structNameField->value == name);
							if (ImGui::Selectable(name.c_str(), selected))
							{
								structNameField->value = name;
								fieldsChanged = true;
							}
							if (selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
				else
				{
					GraphEditorUtils::DrawInspectorReadOnlyField(*structNameField);
				}
			}

			for (NodeField& field : selectedNode->fields)
			{
				if (field.name == "Struct Name" || field.name == "Schema Fields")
					continue;

				if (isInputPinConnectedByName(field.name.c_str()))
				{
					GraphEditorUtils::DrawInspectorReadOnlyField(field);
					continue;
				}

				fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
			}

			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::StructGetField
			  || selectedNode->nodeType == NodeType::StructSetField)
		{
			NodeField* structNameField = GraphEditorUtils::FindField(selectedNode->fields, "Struct Name");
			const NodeField* schemaField = GraphEditorUtils::FindField(selectedNode->fields, "Schema Fields");
			const std::vector<std::string> structNames = CollectDefinedStructNames(state);

			std::vector<std::string> fieldNames;
			if (schemaField)
			{
				for (const auto& def : ParseStructDefs(schemaField->value))
					fieldNames.push_back(def.name);
			}

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
			for (NodeField& field : selectedNode->fields)
			{
				if (field.name == "Schema Fields")
					continue;

				if (field.name == "Struct Name" && structNameField)
				{
					if (!structNames.empty())
					{
						if (std::find(structNames.begin(), structNames.end(), structNameField->value) == structNames.end())
						{
							structNameField->value = structNames.front();
							fieldsChanged = true;
						}

						if (ImGui::BeginCombo("Struct", structNameField->value.c_str()))
						{
							for (const std::string& name : structNames)
							{
								const bool selected = (structNameField->value == name);
								if (ImGui::Selectable(name.c_str(), selected))
								{
									structNameField->value = name;
									fieldsChanged = true;
								}
								if (selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
					else
					{
						GraphEditorUtils::DrawInspectorReadOnlyField(field);
					}
					continue;
				}

				if (field.name == "Field" && !fieldNames.empty())
				{
					if (std::find(fieldNames.begin(), fieldNames.end(), field.value) == fieldNames.end())
					{
						field.value = fieldNames.front();
						fieldsChanged = true;
					}

					if (ImGui::BeginCombo("Field", field.value.c_str()))
					{
						for (const std::string& name : fieldNames)
						{
							const bool selected = (field.value == name);
							if (ImGui::Selectable(name.c_str(), selected))
							{
								field.value = name;
								fieldsChanged = true;
							}
							if (selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					continue;
				}

				if (isInputPinConnectedByName(field.name.c_str()))
				{
					GraphEditorUtils::DrawInspectorReadOnlyField(field);
					continue;
				}

				fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
			}
			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::Loop
			  || selectedNode->nodeType == NodeType::Operator
			  || selectedNode->nodeType == NodeType::Comparison
			  || selectedNode->nodeType == NodeType::Logic
			  || selectedNode->nodeType == NodeType::Not
			  || selectedNode->nodeType == NodeType::Delay)
		{
			auto isInputPinConnected = [&](const char* pinName) -> bool
			{
				Pin* targetPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, pinName);
				if (!targetPin)
					return false;

				for (const Link& l : state.GetLinks())
				{
					if (l.alive && l.endPinId == targetPin->id)
						return true;
				}
				return false;
			};

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
			for (NodeField& field : selectedNode->fields)
			{
				const bool makeReadOnly =
					(selectedNode->nodeType == NodeType::Loop && (field.name == "Start" || field.name == "Count") && isInputPinConnected(field.name.c_str())) ||
					((selectedNode->nodeType == NodeType::Operator || selectedNode->nodeType == NodeType::Comparison || selectedNode->nodeType == NodeType::Logic) && (field.name == "A" || field.name == "B") && isInputPinConnected(field.name.c_str())) ||
					(selectedNode->nodeType == NodeType::Not && field.name == "A" && isInputPinConnected("A")) ||
					(selectedNode->nodeType == NodeType::Delay && field.name == "Duration" && isInputPinConnected("Duration"));

				if (makeReadOnly)
				{
					GraphEditorUtils::DrawInspectorReadOnlyField(field);
					continue;
				}

				fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
			}
			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::While)
		{
			ImGui::TextColored(colors::textSecondary, "While runs body while Condition is true.");
		}
		else
		{
			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
			for (NodeField& field : selectedNode->fields)
				fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
			ImGui::PopID();
		}
	}

	if (fieldsChanged)
	{
		GraphSerializer::ApplyConstantTypeFromFields(*selectedNode, /*resetValueOnTypeChange=*/true);

		GraphEditorUtils::RefreshVariableNodeTypes(state);
		GraphEditorUtils::RefreshForEachNodeLayout(state);
		GraphEditorUtils::RefreshLoopNodeLayout(state);
		GraphEditorUtils::RefreshDrawRectNodeLayout(state);
		GraphEditorUtils::RefreshStructNodeLayouts(state);
		GraphEditorUtils::SyncLinkTypesAndPruneInvalid(state);
		state.MarkDirty();
	}
}
