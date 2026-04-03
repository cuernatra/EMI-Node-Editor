#include "nodeRegistry.h"

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

const std::unordered_map<NodeType, NodeDescriptor>& NodeRegistry::All() const
{
    return descriptors_;
}
