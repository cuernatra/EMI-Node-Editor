#include "nodeRegistry.h"
#include <cctype>
#include <string_view>
#include <unordered_map>

namespace
{
std::string RemoveWhitespace(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s)
    {
        if (!std::isspace(ch))
            out.push_back(static_cast<char>(ch));
    }
    return out;
}

std::string DefaultSaveTokenForType(NodeType type)
{
    switch (type)
    {
        case NodeType::Start: return "Start";
        case NodeType::Constant: return "Constant";
        case NodeType::Operator: return "Operator";
        case NodeType::Comparison: return "Comparison";
        case NodeType::Logic: return "Logic";
        case NodeType::Not: return "Not";
        case NodeType::DrawRect: return "DrawRect";
        case NodeType::DrawGrid: return "DrawGrid";
        case NodeType::Delay: return "Delay";
        case NodeType::Sequence: return "Sequence";
        case NodeType::Branch: return "Branch";
        case NodeType::Loop: return "Loop";
        case NodeType::ForEach: return "ForEach";
        case NodeType::ArrayGetAt: return "ArrayGet";
        case NodeType::ArrayAddAt: return "ArrayAdd";
        case NodeType::ArrayReplaceAt: return "ArrayReplace";
        case NodeType::ArrayRemoveAt: return "ArrayRemove";
        case NodeType::ArrayLength: return "ArrayLength";
        case NodeType::StructDefine: return "StructDefine";
        case NodeType::StructCreate: return "StructCreate";
        case NodeType::While: return "While";
        case NodeType::Variable: return "Variable";
        case NodeType::Function: return "Function";
        case NodeType::Output: return "Output";
        default: return "Unknown";
    }
}
}

NodeRegistry::NodeRegistry()
{
    RegisterEventNodes();
    RegisterDataNodes();
    RegisterLogicNodes();
    RegisterFlowNodes();
    RegisterStructNodes();
}

void NodeRegistry::Register(NodeDescriptor descriptor)
{
    if (descriptor.saveToken.empty())
        descriptor.saveToken = DefaultSaveTokenForType(descriptor.type);

    descriptors_[descriptor.type] = std::move(descriptor);
}

const NodeRegistry& NodeRegistry::Get()
{
    static NodeRegistry instance;
    return instance;
}

const NodeDescriptor* NodeRegistry::Find(NodeType type) const
{
    auto it = descriptors_.find(type);
    return it != descriptors_.end() ? &it->second : nullptr;
}

NodeType NodeRegistry::FindByToken(const std::string& token) const
{
    std::string normalized = RemoveWhitespace(token);

    static const std::unordered_map<std::string, std::string> kAliases = {
        { "ArrayCount", "ArrayLength" },
        { "DebugPrint", "Output" }
    };

    auto aliasIt = kAliases.find(normalized);
    if (aliasIt != kAliases.end())
        normalized = aliasIt->second;

    for (const auto& [type, descriptor] : descriptors_)
    {
        if (RemoveWhitespace(descriptor.saveToken) == normalized)
            return type;
    }

    return NodeType::Unknown;
}

const std::unordered_map<NodeType, NodeDescriptor>& NodeRegistry::All() const
{
    return descriptors_;
}
