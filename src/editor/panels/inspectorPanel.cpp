#include "inspectorPanel.h"

#include "ui/theme.h"

#include "editor/graph/graphState.h"
#include "editor/graph/graphSerializer.h"
#include "editor/graph/graphEditorUtils.h"

#include "editor/renderer/fieldWidgetRenderer.h"

#include "core/registry/nodeRegistry.h"

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

static std::vector<InspectorStructFieldDef> ParseStructDefs(const std::string& raw)
{
	std::vector<InspectorStructFieldDef> defs;
	const std::vector<std::string> items = ParseArrayItems(raw);
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
	std::vector<std::string> items;
	items.reserve(defs.size());
	for (size_t i = 0; i < defs.size(); ++i)
		items.push_back("\"" + defs[i].name + ":" + defs[i].type + "\"");
	return BuildArrayString(items);
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

static bool ContainsString(const std::vector<std::string>& list, const std::string& value)
{
	return std::find(list.begin(), list.end(), value) != list.end();
}

static bool DrawInspectorFieldsFromDescriptor(
	GraphState& state,
	VisualNode& node,
	const NodeDescriptor* desc,
	const std::function<bool(const char*)>& isInputPinConnectedByName,
	const std::vector<std::string>& skipNames,
	const std::function<bool(NodeField&)>& overrideDrawer,
	bool& fieldsChanged)
{
	bool drewAny = false;
	std::vector<std::string> descriptorNames;
	if (desc)
	{
		descriptorNames.reserve(desc->fields.size());
		for (const FieldDescriptor& fd : desc->fields)
			descriptorNames.push_back(fd.name);
	}

	auto shouldSkip = [&](const std::string& name) -> bool
	{
		return ContainsString(skipNames, name);
	};

	auto drawOneField = [&](NodeField& field) -> void
	{
		if (shouldSkip(field.name))
			return;

		if (overrideDrawer)
		{
			const bool handled = overrideDrawer(field);
			if (handled)
			{
				drewAny = true;
				return;
			}
		}

		if (isInputPinConnectedByName(field.name.c_str()))
		{
			DrawFieldReadOnly(field, FieldWidgetLayout::Inspector);
			drewAny = true;
			return;
		}

		fieldsChanged |= DrawField(field, FieldWidgetLayout::Inspector);
		drewAny = true;
	};

	if (desc)
	{
		for (const FieldDescriptor& fd : desc->fields)
		{
			NodeField* field = GraphEditorUtils::FindField(node.fields, fd.name.c_str());
			if (!field)
				continue;
			drawOneField(*field);
		}
	}

	// Draw runtime fields not present in the descriptor (dynamic fields, nodes with extra fields, etc.)
	for (NodeField& field : node.fields)
	{
		if (shouldSkip(field.name))
			continue;

		if (ContainsString(descriptorNames, field.name))
			continue;

		drawOneField(field);
	}

	return drewAny;
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
	const NodeDescriptor* descriptor = NodeRegistry::Get().Find(selectedNode->nodeType);
	const char* typeLabel = descriptor ? descriptor->label.c_str() : "Unknown";

	ImGui::TextColored(colors::textSecondary, "Node ID: %llu", static_cast<unsigned long long>(selectedNode->id.Get()));
	ImGui::TextColored(colors::textSecondary, "Type: %s", typeLabel);
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
			const NodeDescriptor* desc = NodeRegistry::Get().Find(selectedNode->nodeType);

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

			std::vector<std::string> setVariableNames;
			if (isGet)
			{
				for (const auto& node : state.GetNodes())
				{
					if (!node.alive || node.nodeType != NodeType::Variable)
						continue;

					const NodeField* nVariant = GraphEditorUtils::FindField(node.fields, "Variant");
					if (!nVariant || nVariant->value != "Set")
						continue;

					const NodeField* nName = GraphEditorUtils::FindField(node.fields, "Name");
					const std::string varName = nName ? nName->value : "myVar";
					if (!ContainsString(setVariableNames, varName))
						setVariableNames.push_back(varName);
				}
			}

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
			DrawInspectorFieldsFromDescriptor(
				state,
				*selectedNode,
				desc,
				isInputPinConnectedByName,
				/*skipNames=*/{ "Variant" },
				/*overrideDrawer=*/[&](NodeField& field) -> bool
				{
					if (isGet && nameField && &field == nameField)
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
							return true;
						}

						if (!ContainsString(setVariableNames, nameField->value))
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
						return true;
					}

					if (isGet && typeField && &field == typeField)
					{
						DrawFieldReadOnly(field, FieldWidgetLayout::Inspector);
						return true;
					}

					if (!isGet)
					{
						if ((field.name == "Default" && defaultConnected)
							|| isInputPinConnectedByName(field.name.c_str()))
						{
							DrawFieldReadOnly(field, FieldWidgetLayout::Inspector);
							return true;
						}
					}

					return false;
				},
				fieldsChanged);
			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::StructDefine)
		{
			NodeField* nameField = GraphEditorUtils::FindField(selectedNode->fields, "Struct Name");
			NodeField* defsField = GraphEditorUtils::FindField(selectedNode->fields, "Fields");
			const NodeDescriptor* desc = NodeRegistry::Get().Find(selectedNode->nodeType);

			std::vector<InspectorStructFieldDef> defs = ParseStructDefs(defsField ? defsField->value : "[]");

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
			DrawInspectorFieldsFromDescriptor(
				state,
				*selectedNode,
				desc,
				isInputPinConnectedByName,
				/*skipNames=*/{ "Fields" },
				/*overrideDrawer=*/{},
				fieldsChanged);

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
			const NodeDescriptor* desc = NodeRegistry::Get().Find(selectedNode->nodeType);

			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

			DrawInspectorFieldsFromDescriptor(
				state,
				*selectedNode,
				desc,
				isInputPinConnectedByName,
				/*skipNames=*/{ "Schema Fields" },
				/*overrideDrawer=*/[&](NodeField& field) -> bool
				{
					if (!structNameField || &field != structNameField)
						return false;

					if (!structNames.empty())
					{
						if (!ContainsString(structNames, structNameField->value))
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
						DrawFieldReadOnly(*structNameField, FieldWidgetLayout::Inspector);
					}
					return true;
				},
				fieldsChanged);

			ImGui::PopID();
		}
		else if (selectedNode->nodeType == NodeType::While)
		{
			ImGui::TextColored(colors::textSecondary, "While runs body while Condition is true.");
		}
		else
		{
			const NodeDescriptor* desc = NodeRegistry::Get().Find(selectedNode->nodeType);
			ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
			const bool drewAny = DrawInspectorFieldsFromDescriptor(
				state,
				*selectedNode,
				desc,
				isInputPinConnectedByName,
				/*skipNames=*/{},
				/*overrideDrawer=*/{},
				fieldsChanged);
			if (!drewAny)
				ImGui::TextColored(colors::textSecondary, "No values.");
			ImGui::PopID();
		}
	}

	if (fieldsChanged)
	{
		GraphSerializer::ApplyConstantTypeFromFields(*selectedNode, /*resetValueOnTypeChange=*/true);

		GraphEditorUtils::RefreshNodesFromRegistryDescriptors(state);
		GraphEditorUtils::RunAllLayoutRefreshes(state);
		GraphEditorUtils::SyncLinkTypesAndPruneInvalid(state);
		state.MarkDirty();
	}
}
