#include "graphEditorUtils.h"
#include "core/graph/link.h"
#include "core/registry/nodeRegistry.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
std::string TrimCopy(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> ParseArrayItems(const std::string& text)
{
    std::string src = TrimCopy(text);
    if (src.size() >= 2 && src.front() == '[' && src.back() == ']')
        src = src.substr(1, src.size() - 2);

    std::vector<std::string> out;
    std::string current;
    current.reserve(src.size());

    int bracketDepth = 0;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inQuote = false;
    char quoteChar = '\0';
    bool escape = false;

    auto pushCurrent = [&]() {
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
        else if (ch == '(') ++parenDepth;
        else if (ch == ')') parenDepth = std::max(0, parenDepth - 1);
        else if (ch == '{') ++braceDepth;
        else if (ch == '}') braceDepth = std::max(0, braceDepth - 1);

        if (ch == ',' && bracketDepth == 0 && parenDepth == 0 && braceDepth == 0)
        {
            pushCurrent();
            continue;
        }

        current.push_back(ch);
    }

    pushCurrent();
    return out;
}

std::string BuildArrayString(const std::vector<std::string>& items)
{
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i != 0)
            out += ", ";
        out += TrimCopy(items[i]);
    }
    out += "]";
    return out;
}

struct StructFieldDef
{
    std::string name;
    PinType type = PinType::Any;
};

std::vector<StructFieldDef> ParseStructFieldDefs(const std::string& text)
{
    std::vector<StructFieldDef> defs;
    const std::vector<std::string> items = ParseArrayItems(text);
    for (const std::string& raw : items)
    {
        std::string item = TrimCopy(raw);
        if (item.size() >= 2 && ((item.front() == '"' && item.back() == '"') || (item.front() == '\'' && item.back() == '\'')))
            item = item.substr(1, item.size() - 2);

        const size_t sep = item.find(':');
        if (sep == std::string::npos)
            continue;

        StructFieldDef def;
        def.name = TrimCopy(item.substr(0, sep));
        const std::string typeName = TrimCopy(item.substr(sep + 1));
        def.type = ValuePinTypeFromString(typeName, PinType::Number);
        if (!def.name.empty())
            defs.push_back(def);
    }
    return defs;
}

std::string BuildStructFieldDefsString(const std::vector<StructFieldDef>& defs)
{
    std::vector<std::string> items;
    items.reserve(defs.size());
    for (const StructFieldDef& def : defs)
        items.push_back(std::string("\"") + def.name + ":" + ValuePinTypeToString(def.type) + "\"");
    return BuildArrayString(items);
}

NodeField* EnsureField(std::vector<NodeField>& fields, const std::string& name, PinType type, const std::string& defaultValue, bool& changed)
{
    if (NodeField* f = GraphEditorUtils::FindField(fields, name.c_str()))
    {
        if (f->valueType != type)
        {
            f->valueType = type;
            changed = true;
        }
        return f;
    }

    fields.push_back(NodeField{ name, type, defaultValue });
    changed = true;
    return &fields.back();
}

bool TryParseDouble(const std::string& s, double& out);
bool ParseBoolLoose(const std::string& s);

bool TryParseDouble(const std::string& s, double& out)
{
    if (s.empty())
        return false;

    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end && *end == '\0')
    {
        out = v;
        return true;
    }

    return false;
}

bool ParseBoolLoose(const std::string& s)
{
    if (s == "true" || s == "True" || s == "1")
        return true;
    if (s == "false" || s == "False" || s == "0")
        return false;

    double v = 0.0;
    if (TryParseDouble(s, v))
        return v != 0.0;

    return false;
}

std::string DefaultValueForPinType(PinType t)
{
    switch (t)
    {
        case PinType::Boolean: return "false";
        case PinType::String:  return "";
        case PinType::Array:   return "[]";
        case PinType::Number:
        default:               return "0.0";
    }
}

void NormalizeValueForPinType(PinType t, std::string& value)
{
    switch (t)
    {
        case PinType::Boolean:
            value = ParseBoolLoose(value) ? "true" : "false";
            break;

        case PinType::Number:
        {
            double v = 0.0;
            if (TryParseDouble(value, v))
                value = std::to_string(v);
            else
                value = "0.0";
            break;
        }

        case PinType::Array:
            if (value.empty())
                value = "[]";
            break;

        case PinType::String:
        default:
            break;
    }
}

void RemovePinsByNameExcept(std::vector<Pin>& pins, const std::vector<std::string>& keepNames)
{
    pins.erase(
        std::remove_if(pins.begin(), pins.end(), [&](const Pin& p)
        {
            for (const std::string& keep : keepNames)
                if (p.name == keep)
                    return false;
            return true;
        }),
        pins.end()
    );
}

const Pin* FindIncomingPinSource(const GraphState& state, const std::vector<Link>& links, const Pin& inputPin)
{
    for (const Link& l : links)
    {
        if (!l.alive || l.endPinId != inputPin.id)
            continue;
        return state.FindPin(l.startPinId);
    }
    return nullptr;
}

bool IsPinLinked(const GraphState& state, ed::PinId pinId)
{
    for (const Link& l : state.GetLinks())
    {
        if (!l.alive)
            continue;
        if (l.startPinId == pinId || l.endPinId == pinId)
            return true;
    }
    return false;
}

enum class DescriptorSyncMode
{
    EnsureOnly,         // Ensure descriptor pins exist; keep extras.
    PruneUnlinkedExtras // Ensure descriptor pins exist; drop extra pins if they are unlinked.
};

DescriptorSyncMode DescriptorSyncModeForStyle(NodeRenderStyle style)
{
    switch (style)
    {
        case NodeRenderStyle::Sequence:
            return DescriptorSyncMode::EnsureOnly; // expandable Then pins
        default:
            return DescriptorSyncMode::PruneUnlinkedExtras;
    }
}

bool ShouldSkipDescriptorSync(NodeRenderStyle style)
{
    switch (style)
    {
        case NodeRenderStyle::Variable:
        case NodeRenderStyle::StructDefine:
        case NodeRenderStyle::StructCreate:
            return true; // handled by bespoke refresh passes

        default:
            return false;
    }
}

bool SyncPinsToDescriptor(GraphState& state,
                         VisualNode& node,
                         const NodeDescriptor& desc,
                         bool isInput,
                         std::vector<Pin>& pins,
                         DescriptorSyncMode mode,
                         bool& changed)
{
    std::vector<Pin> newPins;
    newPins.reserve(pins.size());

    std::vector<ed::PinId> usedIds;
    usedIds.reserve(pins.size());

    for (const PinDescriptor& pd : desc.pins)
    {
        if (pd.isInput != isInput)
            continue;

        Pin* existing = GraphEditorUtils::FindPinByName(pins, pd.name.c_str());
        if (!existing)
        {
            newPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                node.id,
                node.nodeType,
                pd.name,
                pd.type,
                pd.isInput,
                pd.isMultiInput
            ));
            changed = true;
            continue;
        }

        Pin updated = *existing;
        usedIds.push_back(updated.id);

        if (updated.parentNodeId != node.id)
        {
            updated.parentNodeId = node.id;
            changed = true;
        }
        if (updated.parentNodeType != node.nodeType)
        {
            updated.parentNodeType = node.nodeType;
            changed = true;
        }
        if (updated.isInput != pd.isInput)
        {
            updated.isInput = pd.isInput;
            changed = true;
        }
        if (updated.isMultiInput != pd.isMultiInput)
        {
            updated.isMultiInput = pd.isMultiInput;
            changed = true;
        }
        if (updated.name != pd.name)
        {
            updated.name = pd.name;
            changed = true;
        }

        // Avoid fighting dynamic typing:
        // if the descriptor says Any, keep the current runtime-resolved type.
        if (pd.type != PinType::Any && updated.type != pd.type)
        {
            updated.type = pd.type;
            changed = true;
        }

        newPins.push_back(std::move(updated));
    }

    for (const Pin& oldPin : pins)
    {
        const bool used = std::any_of(usedIds.begin(), usedIds.end(), [&](ed::PinId id) { return id == oldPin.id; });
        if (used)
            continue;

        if (mode == DescriptorSyncMode::EnsureOnly)
        {
            newPins.push_back(oldPin);
            continue;
        }

        if (IsPinLinked(state, oldPin.id))
        {
            newPins.push_back(oldPin);
            continue;
        }

        changed = true; // pruned
    }

    if (newPins.size() != pins.size())
        changed = true;

    pins = std::move(newPins);
    return true;
}

bool SyncNodeToDescriptor(GraphState& state, VisualNode& node, const NodeDescriptor& desc)
{
    bool changed = false;

    if (node.title != desc.label)
    {
        node.title = desc.label;
        changed = true;
    }

    // Ensure descriptor fields exist and have correct types.
    for (const FieldDescriptor& fd : desc.fields)
    {
        NodeField* f = GraphEditorUtils::FindField(node.fields, fd.name.c_str());
        if (!f)
        {
            node.fields.push_back(NodeField{ fd.name, fd.valueType, fd.defaultValue, fd.options });
            changed = true;
            continue;
        }

        if (f->options != fd.options)
        {
            f->options = fd.options;
            changed = true;
        }

        // Do not force dynamic Any fields back to descriptor defaults every frame.
        // This is critical for nodes like Constant where runtime/UI resolves the
        // effective value type from a separate type selector.
        if (fd.valueType != PinType::Any && f->valueType != fd.valueType)
        {
            f->valueType = fd.valueType;
            f->value = fd.defaultValue;
            changed = true;
        }

        if (fd.valueType != PinType::Any)
        {
            const std::string before = f->value;
            NormalizeValueForPinType(fd.valueType, f->value);
            if (f->value != before)
                changed = true;
        }
    }

    const DescriptorSyncMode mode = DescriptorSyncModeForStyle(desc.renderStyle);
    SyncPinsToDescriptor(state, node, desc, /*isInput=*/true, node.inPins, mode, changed);
    SyncPinsToDescriptor(state, node, desc, /*isInput=*/false, node.outPins, mode, changed);
    return changed;
}
}

namespace GraphEditorUtils
{
ImVec2 SnapToNodeGrid(const ImVec2& pos)
{
    constexpr float kGridStep = 32.0f;
    return ImVec2(
        std::round(pos.x / kGridStep) * kGridStep,
        std::round(pos.y / kGridStep) * kGridStep
    );
}

void ParseSpawnPayloadTitle(const char* payloadTitle, NodeType& outType, std::string& outVariableVariant)
{
    outType = NodeType::Unknown;
    outVariableVariant.clear();

    if (!payloadTitle)
        return;

    const std::string raw(payloadTitle);
    const size_t sep = raw.find(':');
    const std::string base = (sep == std::string::npos) ? raw : raw.substr(0, sep);

    outType = NodeRegistry::Get().FindByToken(base);
    if (outType == NodeType::Unknown || sep == std::string::npos)
        return;

    const NodeDescriptor* desc = NodeRegistry::Get().Find(outType);
    if (desc && !desc->paletteVariants.empty())
        outVariableVariant = raw.substr(sep + 1);
}

NodeField* FindField(std::vector<NodeField>& fields, const char* name)
{
    for (NodeField& f : fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

const NodeField* FindField(const std::vector<NodeField>& fields, const char* name)
{
    for (const NodeField& f : fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

Pin* FindPinByName(std::vector<Pin>& pins, const char* name)
{
    for (Pin& p : pins)
        if (p.name == name)
            return &p;
    return nullptr;
}

bool RefreshNodesFromRegistryDescriptors(GraphState& state)
{
    bool changed = false;
    auto& nodes = state.GetNodes();

    for (auto& n : nodes)
    {
        if (!n.alive)
            continue;

        const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);
        if (!desc)
            continue;

        if (ShouldSkipDescriptorSync(desc->renderStyle))
            continue;

        changed |= SyncNodeToDescriptor(state, n, *desc);
    }

    return changed;
}

bool RefreshVariableNodeTypes(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();
    const auto& links = state.GetLinks();

    const NodeDescriptor* variableDesc = NodeRegistry::Get().Find(NodeType::Variable);

    struct VariableDef
    {
        PinType type = PinType::Number;
        std::string typeName = "Number";
    };

    std::unordered_map<std::string, VariableDef> definitions;

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Variable)
            continue;

        NodeField* variantField = FindField(n.fields, "Variant");
        NodeField* nameField = FindField(n.fields, "Name");
        NodeField* typeField = FindField(n.fields, "Type");
        NodeField* defaultField = FindField(n.fields, "Default");
        const std::string variant = variantField ? variantField->value : "Set";
        if (variant != "Set")
            continue;

        if (variantField && variantField->value != "Set")
        {
            variantField->value = "Set";
            changed = true;
        }

        // Use registry descriptor to keep the Set-variable pin layout stable.
        if (variableDesc)
        {
            if (n.title != variableDesc->label)
            {
                n.title = variableDesc->label;
                changed = true;
            }

            bool pinsChanged = false;
            const DescriptorSyncMode mode = DescriptorSyncModeForStyle(variableDesc->renderStyle);
            SyncPinsToDescriptor(state, n, *variableDesc, /*isInput=*/true, n.inPins, mode, pinsChanged);
            SyncPinsToDescriptor(state, n, *variableDesc, /*isInput=*/false, n.outPins, mode, pinsChanged);
            if (pinsChanged)
                changed = true;
        }

        Pin* setPin = FindPinByName(n.inPins, "Default");
        Pin* valuePin = FindPinByName(n.outPins, "Value");

        // Ensure core fields exist (do not force Default field type; it is dynamic).
        EnsureField(n.fields, "Variant", PinType::String, "Set", changed);
        EnsureField(n.fields, "Name", PinType::String, "myVar", changed);
        EnsureField(n.fields, "Type", PinType::String, "Number", changed);
        if (!defaultField)
        {
            n.fields.push_back(NodeField{ "Default", PinType::Number, "0.0" });
            changed = true;
        }

        // Reacquire field pointers after EnsureField/push_back operations,
        // because vector growth can invalidate previously captured pointers.
        variantField = FindField(n.fields, "Variant");
        nameField = FindField(n.fields, "Name");
        typeField = FindField(n.fields, "Type");
        defaultField = FindField(n.fields, "Default");

        PinType resolvedType = ValuePinTypeFromString(typeField ? typeField->value : "Number", PinType::Number);
        if (setPin)
        {
            if (const Pin* source = FindIncomingPinSource(state, links, *setPin))
            {
                if (source->type != PinType::Any && source->type != PinType::Flow)
                    resolvedType = source->type;
            }
        }

        const char* resolvedTypeName = ValuePinTypeToString(resolvedType);
        if (typeField && typeField->value != resolvedTypeName)
        {
            typeField->value = resolvedTypeName;
            changed = true;
        }

        if (defaultField)
        {
            const bool typeChanged = (defaultField->valueType != resolvedType);
            // On file load, descriptor bootstrap may leave Variable Default field
            // temporarily as String metadata even when logical variable type is
            // Number/Boolean/Array. Treat that as metadata repair, not a user
            // type-change, so persisted value is preserved.
            const bool metadataRepairOnly =
                typeChanged && (defaultField->valueType == PinType::String);
            defaultField->valueType = resolvedType;

            if (typeChanged)
            {
                if (metadataRepairOnly)
                {
                    const std::string before = defaultField->value;
                    NormalizeValueForPinType(resolvedType, defaultField->value);
                    if (defaultField->value != before)
                        changed = true;
                }
                else
                {
                    defaultField->value = DefaultValueForPinType(resolvedType);
                    changed = true;
                }
            }
            else
            {
                const std::string before = defaultField->value;
                NormalizeValueForPinType(resolvedType, defaultField->value);
                if (defaultField->value != before)
                    changed = true;
            }
        }

        if (setPin && setPin->type != resolvedType)
        {
            setPin->type = resolvedType;
            changed = true;
        }
        if (valuePin && valuePin->type != resolvedType)
        {
            valuePin->type = resolvedType;
            changed = true;
        }

        const std::string varName = nameField ? nameField->value : "myVar";
        definitions[varName] = VariableDef{
            resolvedType,
            resolvedTypeName
        };
    }

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Variable)
            continue;

        NodeField* variantField = FindField(n.fields, "Variant");
        NodeField* nameField = FindField(n.fields, "Name");
        NodeField* typeField = FindField(n.fields, "Type");
        const std::string variant = variantField ? variantField->value : "Set";
        const std::string varName = nameField ? nameField->value : "";

        if (variant == "Get")
        {
            if (variantField && variantField->value != "Get")
            {
                variantField->value = "Get";
                changed = true;
            }

            if (n.title != "Get Variable")
            {
                n.title = "Get Variable";
                changed = true;
            }

            if (nameField && definitions.empty())
            {
                if (!nameField->value.empty())
                {
                    nameField->value.clear();
                    changed = true;
                }
            }
            else if (nameField && !definitions.empty() && definitions.find(nameField->value) == definitions.end())
            {
                nameField->value = definitions.begin()->first;
                changed = true;
            }

            const std::string resolvedName = nameField ? nameField->value : "";
            const auto it = definitions.find(resolvedName);
            const PinType getType = (it != definitions.end())
                ? it->second.type
                : ValuePinTypeFromString(typeField ? typeField->value : "Number", PinType::Number);

            const char* getTypeName = ValuePinTypeToString(getType);
            if (typeField && typeField->value != getTypeName)
            {
                typeField->value = getTypeName;
                changed = true;
            }

            if (!n.inPins.empty())
            {
                n.inPins.clear();
                changed = true;
            }

            const size_t outBefore = n.outPins.size();
            RemovePinsByNameExcept(n.outPins, { "Value" });
            if (n.outPins.size() != outBefore)
                changed = true;

            Pin* valuePin = FindPinByName(n.outPins, "Value");
            if (!valuePin)
            {
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                    n.id,
                    n.nodeType,
                    "Value",
                    getType,
                    false
                ));
                valuePin = &n.outPins.back();
                changed = true;
            }
            if (valuePin && valuePin->type != getType)
            {
                valuePin->type = getType;
                changed = true;
            }
        }
        else
        {
            if (variantField && variantField->value != "Set")
            {
                variantField->value = "Set";
                changed = true;
            }

            if (n.title != "Set Variable")
            {
                n.title = "Set Variable";
                changed = true;
            }

            const size_t inBefore = n.inPins.size();
            RemovePinsByNameExcept(n.inPins, { "In", "Default" });
            if (n.inPins.size() != inBefore)
                changed = true;

            const size_t outBefore = n.outPins.size();
            RemovePinsByNameExcept(n.outPins, { "Out", "Value" });
            if (n.outPins.size() != outBefore)
                changed = true;
        }
    }

    return changed;
}

bool RefreshOutputNodeInputTypes(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();
    const auto& links = state.GetLinks();

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Output)
            continue;

        Pin* valuePin = FindPinByName(n.inPins, "Value");
        if (!valuePin)
            continue;

        PinType resolvedType = PinType::Any;
        for (const Link& l : links)
        {
            if (!l.alive || l.endPinId != valuePin->id)
                continue;

            const Pin* src = state.FindPin(l.startPinId);
            if (src && src->type != PinType::Flow)
            {
                resolvedType = src->type;
                break;
            }
        }

        if (valuePin->type != resolvedType)
        {
            valuePin->type = resolvedType;
            changed = true;
        }
    }

    return changed;
}

bool RefreshForEachNodeLayout(GraphState& state)
{
    bool changed = false;
    auto& nodes = state.GetNodes();

    const NodeDescriptor* desc = NodeRegistry::Get().Find(NodeType::ForEach);
    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::ForEach)
            continue;

        if (desc)
            changed |= SyncNodeToDescriptor(state, n, *desc);

        NodeField* elementTypeField = FindField(n.fields, "Element Type");
        if (elementTypeField)
        {
            const PinType parsedType = ValuePinTypeFromString(elementTypeField->value, PinType::Number);
            const std::string normalized = ValuePinTypeToString(parsedType);
            if (elementTypeField->value != normalized)
            {
                elementTypeField->value = normalized;
                changed = true;
            }

            const PinType elementType = ValuePinTypeFromString(elementTypeField->value, PinType::Number);
            if (Pin* elementPin = FindPinByName(n.outPins, "Element"))
            {
                if (elementPin->type != elementType)
                {
                    elementPin->type = elementType;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

bool RefreshStructNodeLayouts(GraphState& state)
{
    auto& nodes = state.GetNodes();

    static const std::vector<VisualNode>* s_lastNodesPtr = nullptr;
    static size_t s_lastAliveStructDefineCount = 0;
    static size_t s_lastAliveStructShapeHash = 0;

    size_t aliveStructDefineCount = 0;
    size_t aliveStructShapeHash = 1469598103934665603ull; // FNV offset basis

    for (const auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::StructDefine)
            continue;

        ++aliveStructDefineCount;

        // Mix stable, cheap shape inputs only (avoid full schema parsing every frame).
        aliveStructShapeHash ^= static_cast<size_t>(n.id.Get());
        aliveStructShapeHash *= 1099511628211ull;
        aliveStructShapeHash ^= n.fields.size();
        aliveStructShapeHash *= 1099511628211ull;
        aliveStructShapeHash ^= n.inPins.size();
        aliveStructShapeHash *= 1099511628211ull;
        aliveStructShapeHash ^= n.outPins.size();
        aliveStructShapeHash *= 1099511628211ull;
    }

    const bool structLikelyDirty =
        (s_lastNodesPtr != &nodes)
        || (s_lastAliveStructDefineCount != aliveStructDefineCount)
        || (s_lastAliveStructShapeHash != aliveStructShapeHash);

    s_lastNodesPtr = &nodes;
    s_lastAliveStructDefineCount = aliveStructDefineCount;
    s_lastAliveStructShapeHash = aliveStructShapeHash;

    if (!structLikelyDirty)
        return false;

    bool changed = false;

    struct StructDefEntry
    {
        std::vector<StructFieldDef> defs;
        std::string schemaText;
    };

    std::unordered_map<std::string, StructDefEntry> definitions;

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::StructDefine)
            continue;

        EnsureField(n.fields, "Struct Name", PinType::String, "test", changed);
        EnsureField(n.fields, "Fields", PinType::Array, "[]", changed);

        // Reacquire pointers after EnsureField calls because vector growth
        // can invalidate prior pointers.
        NodeField* nameField = GraphEditorUtils::FindField(n.fields, "Struct Name");
        NodeField* fieldsField = GraphEditorUtils::FindField(n.fields, "Fields");

        const std::vector<StructFieldDef> defs = ParseStructFieldDefs(fieldsField ? fieldsField->value : "[]");
        const std::string schemaName = (nameField && !nameField->value.empty()) ? nameField->value : "test";
        const std::string wantedTitle = "Struct " + schemaName;
        if (n.title != wantedTitle)
        {
            n.title = wantedTitle;
            changed = true;
        }

        RemovePinsByNameExcept(n.inPins, {});
        RemovePinsByNameExcept(n.outPins, { "Schema" });
        Pin* schemaPin = FindPinByName(n.outPins, "Schema");
        if (!schemaPin)
        {
            n.outPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Schema", PinType::Array, false));
            changed = true;
        }
        else if (schemaPin->type != PinType::Array)
        {
            schemaPin->type = PinType::Array;
            changed = true;
        }

        definitions[schemaName] = StructDefEntry{ defs, BuildStructFieldDefsString(defs) };
    }

    auto ensureSchemaFields = [&](VisualNode& n, const std::string& defaultName) -> std::pair<std::string, StructDefEntry>
    {
        EnsureField(n.fields, "Struct Name", PinType::String, defaultName, changed);
        EnsureField(n.fields, "Schema Fields", PinType::Array, "[]", changed);

        // Reacquire pointers after EnsureField mutations to avoid dangling refs.
        NodeField* nameField = GraphEditorUtils::FindField(n.fields, "Struct Name");
        NodeField* schemaField = GraphEditorUtils::FindField(n.fields, "Schema Fields");

        std::string structName = (nameField && !nameField->value.empty()) ? nameField->value : defaultName;
        auto it = definitions.find(structName);
        if (it == definitions.end() && !definitions.empty())
        {
            structName = definitions.begin()->first;
            if (nameField)
            {
                nameField->value = structName;
                changed = true;
            }
            it = definitions.find(structName);
        }

        StructDefEntry entry;
        if (it != definitions.end())
            entry = it->second;

        const std::string targetSchema = entry.schemaText.empty() ? "[]" : entry.schemaText;
        if (schemaField && schemaField->value != targetSchema)
        {
            schemaField->value = targetSchema;
            changed = true;
        }

        return { structName, entry };
    };

    for (auto& n : nodes)
    {
        if (!n.alive)
            continue;

        if (n.nodeType == NodeType::StructCreate)
        {
            auto [structName, entry] = ensureSchemaFields(n, "test");
            const std::string wantedTitle = "Create " + structName;
            if (n.title != wantedTitle)
            {
                n.title = wantedTitle;
                changed = true;
            }

            RemovePinsByNameExcept(n.outPins, { "Item" });

            {
                std::vector<NodeField> filteredFields;
                filteredFields.reserve(n.fields.size());
                for (const NodeField& field : n.fields)
                {
                    if (field.name == "Struct Name" || field.name == "Schema Fields")
                    {
                        filteredFields.push_back(field);
                        continue;
                    }

                    const bool existsInSchema = std::any_of(
                        entry.defs.begin(),
                        entry.defs.end(),
                        [&](const StructFieldDef& def) { return def.name == field.name; }
                    );

                    if (existsInSchema)
                        filteredFields.push_back(field);
                    else
                        changed = true;
                }

                if (filteredFields.size() != n.fields.size())
                    n.fields = std::move(filteredFields);
            }

            Pin* structPin = FindPinByName(n.inPins, "Struct");
            if (!structPin)
            {
                n.inPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Struct", PinType::String, true));
                changed = true;
            }
            else if (structPin->type != PinType::String)
            {
                structPin->type = PinType::String;
                changed = true;
            }

            std::unordered_set<uintptr_t> claimedInputPinIds;
            if (structPin)
                claimedInputPinIds.insert(static_cast<uintptr_t>(structPin->id.Get()));

            auto isSchemaFieldName = [&](const std::string& pinName) -> bool
            {
                return std::any_of(
                    entry.defs.begin(),
                    entry.defs.end(),
                    [&](const StructFieldDef& def) { return def.name == pinName; }
                );
            };

            auto findReusableLegacyInputPin = [&]() -> Pin*
            {
                for (Pin& p : n.inPins)
                {
                    if (p.name == "Struct")
                        continue;

                    const uintptr_t pid = static_cast<uintptr_t>(p.id.Get());
                    if (claimedInputPinIds.find(pid) != claimedInputPinIds.end())
                        continue;

                    // Prefer legacy placeholders produced by older deserialization
                    // ("Field N"). Fall back to any non-schema, non-Struct input.
                    const bool isLegacyPlaceholder = (p.name.rfind("Field ", 0) == 0);
                    const bool isSchemaNamed = isSchemaFieldName(p.name);
                    if (isLegacyPlaceholder || !isSchemaNamed)
                        return &p;
                }
                return nullptr;
            };

            for (const StructFieldDef& def : entry.defs)
            {
                Pin* fieldPin = FindPinByName(n.inPins, def.name.c_str());
                if (!fieldPin)
                {
                    if (Pin* reusable = findReusableLegacyInputPin())
                    {
                        if (reusable->name != def.name)
                        {
                            reusable->name = def.name;
                            changed = true;
                        }
                        fieldPin = reusable;
                    }
                    else
                    {
                        n.inPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, def.name, def.type, true));
                        fieldPin = &n.inPins.back();
                        changed = true;
                    }
                }

                if (fieldPin && fieldPin->type != def.type)
                {
                    fieldPin->type = def.type;
                    changed = true;
                }

                if (fieldPin)
                    claimedInputPinIds.insert(static_cast<uintptr_t>(fieldPin->id.Get()));

                NodeField* valueField = EnsureField(n.fields, def.name, def.type, DefaultValueForPinType(def.type), changed);
                const std::string before = valueField->value;
                NormalizeValueForPinType(def.type, valueField->value);
                if (valueField->value != before)
                    changed = true;
            }

            // Prune stale dynamic input pins only when they are unlinked.
            // Keep linked extras to avoid dropping user wires during live schema edits.
            {
                n.inPins.erase(
                    std::remove_if(n.inPins.begin(), n.inPins.end(), [&](const Pin& p)
                    {
                        if (p.name == "Struct")
                            return false;

                        const bool existsInSchema = std::any_of(
                            entry.defs.begin(),
                            entry.defs.end(),
                            [&](const StructFieldDef& def) { return def.name == p.name; }
                        );

                        if (existsInSchema)
                            return false;

                        if (IsPinLinked(state, p.id))
                            return false;

                        changed = true;
                        return true;
                    }),
                    n.inPins.end()
                );
            }

            Pin* itemPin = FindPinByName(n.outPins, "Item");
            if (!itemPin)
            {
                n.outPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Item", PinType::Array, false));
                changed = true;
            }
            else if (itemPin->type != PinType::Array)
            {
                itemPin->type = PinType::Array;
                changed = true;
            }
        }
    }

    return changed;
}

bool RunAllLayoutRefreshes(GraphState& state)
{
    using LayoutRefreshFn = bool(*)(GraphState&);
    struct LayoutRefreshEntry
    {
        NodeType type;
        LayoutRefreshFn fn;
    };

    static const LayoutRefreshEntry kLayoutRefreshTable[] = {
        { NodeType::Variable,     RefreshVariableNodeTypes },
        { NodeType::ForEach,      RefreshForEachNodeLayout },
        { NodeType::Output,       RefreshOutputNodeInputTypes },
        { NodeType::StructDefine, RefreshStructNodeLayouts },
        { NodeType::StructCreate, RefreshStructNodeLayouts },
    };

    bool changed = false;
    std::unordered_set<LayoutRefreshFn> seen;
    for (const LayoutRefreshEntry& entry : kLayoutRefreshTable)
    {
        (void)entry.type;
        if (seen.insert(entry.fn).second)
            changed |= entry.fn(state);
    }

    return changed;
}

bool SyncLinkTypesAndPruneInvalid(GraphState& state)
{
    bool changed = false;
    auto& links = state.GetLinks();

    links.erase(
        std::remove_if(links.begin(), links.end(), [&](const Link& l)
        {
            const Pin* start = state.FindPin(l.startPinId);
            const Pin* end   = state.FindPin(l.endPinId);
            if (!start || !end)
            {
                changed = true;
                return true;
            }

            const Pin* outPin = !start->isInput ? start : end;
            const Pin* inPin  = start->isInput ? start : end;

            if (!Pin::CanConnect(*outPin, *inPin))
            {
                changed = true;
                return true;
            }

            return false;
        }),
        links.end()
    );

    for (Link& l : links)
    {
        const Pin* start = state.FindPin(l.startPinId);
        if (!start)
            continue;

        if (l.type != start->type)
        {
            l.type = start->type;
            changed = true;
        }
    }

    return changed;
}

void DisconnectNonAnyLinksForPins(GraphState& state, const std::vector<ed::PinId>& pinIds)
{
    if (pinIds.empty())
        return;

    auto touchesChangedPin = [&](const Link& l)
    {
        for (ed::PinId id : pinIds)
        {
            if (l.startPinId == id || l.endPinId == id)
                return true;
        }
        return false;
    };

    auto& links = state.GetLinks();
    const size_t before = links.size();

    links.erase(
        std::remove_if(links.begin(), links.end(), [&](const Link& l)
        {
            if (!touchesChangedPin(l))
                return false;

            return l.type != PinType::Any;
        }),
        links.end()
    );

    if (links.size() != before)
        state.MarkDirty();
}
}