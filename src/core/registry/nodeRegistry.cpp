#include "nodeRegistry.h"
#include <cctype>
#include <algorithm>
#include <stdexcept>
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

}

NodeRegistry::NodeRegistry()
{
    RegisterEventNodes();
    RegisterDataNodes();
    RegisterLogicNodes();
    RegisterFlowNodes();
    RegisterStructNodes();
    RegisterDemoNodes();
    RegisterRenderNodes();
}

void NodeRegistry::Register(NodeDescriptor descriptor)
{
    // Save token is required for reliable load/spawn lookup.
    if (descriptor.saveToken.empty())
    {
        throw std::invalid_argument(
            "NodeRegistry::Register: descriptor '" + descriptor.label +
            "' is missing required saveToken"
        );
    }

    for (const std::string& pinName : descriptor.deferredInputPins)
    {
        if (pinName == "*")
            continue;

        const auto pinIt = std::find_if(descriptor.pins.begin(), descriptor.pins.end(), [&](const PinDescriptor& pin)
        {
            return pin.isInput && pin.name == pinName;
        });

        if (pinIt == descriptor.pins.end())
        {
            throw std::invalid_argument(
                "NodeRegistry::Register: descriptor '" + descriptor.label +
                "' references unknown deferred input pin '" + pinName + "'"
            );
        }

        const auto fieldIt = std::find_if(descriptor.fields.begin(), descriptor.fields.end(), [&](const FieldDescriptor& field)
        {
            return field.name == pinName;
        });

        if (fieldIt == descriptor.fields.end())
        {
            throw std::invalid_argument(
                "NodeRegistry::Register: descriptor '" + descriptor.label +
                "' references deferred input pin '" + pinName + "' without a same-name field"
            );
        }
    }

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
