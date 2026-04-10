#include <catch2/catch_test_macros.hpp>

#include "core/compiler/graphCompiler.h"
#include "core/registry/nodeFactory.h"
#include "core/registry/nodeRegistry.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace
{
Pin* FindPin(std::vector<Pin>& pins, const char* name)
{
    for (Pin& pin : pins)
    {
        if (pin.name == name)
            return &pin;
    }
    return nullptr;
}

NodeField* FindField(std::vector<NodeField>& fields, const char* name)
{
    for (NodeField& field : fields)
    {
        if (field.name == name)
            return &field;
    }
    return nullptr;
}

Link MakeLink(IdGen& gen, ed::PinId startPinId, ed::PinId endPinId, PinType type)
{
    Link link;
    link.id = gen.NewLink();
    link.startPinId = startPinId;
    link.endPinId = endPinId;
    link.type = type;
    return link;
}

Node* FunctionBody(Node* root)
{
    if (!root || root->children.empty())
        return nullptr;

    Node* functionDef = root->children[0];
    if (!functionDef || functionDef->type != Token::FunctionDef || functionDef->children.size() < 2)
        return nullptr;

    return functionDef->children[1];
}
}

TEST_CASE("GraphCompiler compiles Start -> Output with value input", "[compiler]")
{
    IdGen gen;
    std::vector<VisualNode> nodes;
    std::vector<Link> links;

    nodes.push_back(CreateNodeFromType(NodeType::Start, gen, ImVec2(0.0f, 0.0f)));
    nodes.push_back(CreateNodeFromType(NodeType::Constant, gen, ImVec2(200.0f, 0.0f)));
    nodes.push_back(CreateNodeFromType(NodeType::Output, gen, ImVec2(400.0f, 0.0f)));

    VisualNode& start = nodes[0];
    VisualNode& constant = nodes[1];
    VisualNode& output = nodes[2];

    NodeField* typeField = FindField(constant.fields, "Type");
    NodeField* valueField = FindField(constant.fields, "Value");
    REQUIRE(typeField != nullptr);
    REQUIRE(valueField != nullptr);
    typeField->value = "Number";
    valueField->value = "42";

    Pin* startExec = FindPin(start.outPins, "Exec");
    Pin* outputIn = FindPin(output.inPins, "In");
    Pin* constantOut = FindPin(constant.outPins, "Value");
    Pin* outputValue = FindPin(output.inPins, "Value");

    REQUIRE(startExec != nullptr);
    REQUIRE(outputIn != nullptr);
    REQUIRE(constantOut != nullptr);
    REQUIRE(outputValue != nullptr);

    links.push_back(MakeLink(gen, startExec->id, outputIn->id, PinType::Flow));
    links.push_back(MakeLink(gen, constantOut->id, outputValue->id, PinType::Number));

    GraphCompiler compiler;
    Node* ast = compiler.Compile(nodes, links);

    REQUIRE(ast != nullptr);
    REQUIRE_FALSE(compiler.HasError);
    REQUIRE(ast->type == Token::Scope);

    Node* body = FunctionBody(ast);
    REQUIRE(body != nullptr);
    REQUIRE(body->type == Token::Scope);
    REQUIRE_FALSE(body->children.empty());

    delete ast;
}

TEST_CASE("GraphCompiler compiles ForEach flow and binds foreach element var", "[compiler]")
{
    IdGen gen;
    std::vector<VisualNode> nodes;
    std::vector<Link> links;

    nodes.push_back(CreateNodeFromType(NodeType::Start, gen, ImVec2(0.0f, 0.0f)));
    nodes.push_back(CreateNodeFromType(NodeType::Constant, gen, ImVec2(200.0f, 0.0f)));
    nodes.push_back(CreateNodeFromType(NodeType::ForEach, gen, ImVec2(400.0f, 0.0f)));

    VisualNode& start = nodes[0];
    VisualNode& constant = nodes[1];
    VisualNode& forEach = nodes[2];

    NodeField* typeField = FindField(constant.fields, "Type");
    NodeField* valueField = FindField(constant.fields, "Value");
    REQUIRE(typeField != nullptr);
    REQUIRE(valueField != nullptr);
    typeField->value = "Array";
    valueField->value = "[1,2,3]";

    Pin* startExec = FindPin(start.outPins, "Exec");
    Pin* forEachIn = FindPin(forEach.inPins, "In");
    Pin* constantOut = FindPin(constant.outPins, "Value");
    Pin* forEachArray = FindPin(forEach.inPins, "Array");

    REQUIRE(startExec != nullptr);
    REQUIRE(forEachIn != nullptr);
    REQUIRE(constantOut != nullptr);
    REQUIRE(forEachArray != nullptr);

    links.push_back(MakeLink(gen, startExec->id, forEachIn->id, PinType::Flow));
    links.push_back(MakeLink(gen, constantOut->id, forEachArray->id, PinType::Array));

    GraphCompiler compiler;
    Node* ast = compiler.Compile(nodes, links);

    REQUIRE(ast != nullptr);
    REQUIRE_FALSE(compiler.HasError);

    Node* body = FunctionBody(ast);
    REQUIRE(body != nullptr);
    REQUIRE(body->type == Token::Scope);

    bool foundForEachFor = false;
    for (Node* stmt : body->children)
    {
        if (!stmt || stmt->type != Token::For)
            continue;

        foundForEachFor = true;
        const std::string* varName = std::get_if<std::string>(&stmt->data);
        REQUIRE(varName != nullptr);
        REQUIRE(varName->find("__foreach_elem_") == 0);
        REQUIRE(stmt->children.size() >= 3);
        break;
    }

    REQUIRE(foundForEachFor);
    delete ast;
}

TEST_CASE("All current NodeTypes are registered and compile callbacks are valid", "[registry][compiler]")
{
    const std::vector<NodeType> expectedTypes = {
        NodeType::Start,
        NodeType::Constant,
        NodeType::Operator,
        NodeType::Comparison,
        NodeType::Logic,
        NodeType::Not,
        NodeType::DrawRect,
        NodeType::DrawGrid,
        NodeType::Delay,
        NodeType::Sequence,
        NodeType::Branch,
        NodeType::Loop,
        NodeType::ForEach,
        NodeType::ArrayGetAt,
        NodeType::ArrayAddAt,
        NodeType::ArrayReplaceAt,
        NodeType::ArrayRemoveAt,
        NodeType::ArrayLength,
        NodeType::StructDefine,
        NodeType::StructCreate,
        NodeType::StructRead,
        NodeType::StructWrite,
        NodeType::PreviewPickRect,
        NodeType::While,
        NodeType::Variable,
        NodeType::Function,
        NodeType::Output
    };

    const NodeRegistry& registry = NodeRegistry::Get();
    const auto& all = registry.All();

    REQUIRE(all.size() == expectedTypes.size());

    std::unordered_set<NodeType> expectedSet(expectedTypes.begin(), expectedTypes.end());
    for (const auto& [type, _desc] : all)
        REQUIRE(expectedSet.find(type) != expectedSet.end());

    IdGen gen;
    for (NodeType type : expectedTypes)
    {
        const NodeDescriptor* desc = registry.Find(type);
        REQUIRE(desc != nullptr);
        REQUIRE(desc->compile != nullptr);
        REQUIRE_FALSE(desc->saveToken.empty());
        REQUIRE(registry.FindByToken(desc->saveToken) == type);

        VisualNode node = CreateNodeFromType(type, gen, ImVec2(0.0f, 0.0f));
        REQUIRE(node.nodeType == type);
        REQUIRE(node.title == desc->label);

        GraphCompiler compiler;
        Node* compiled = desc->compile(&compiler, node);
        REQUIRE_FALSE(compiler.HasError);
        REQUIRE(compiled != nullptr);
        delete compiled;
    }
}

TEST_CASE("Registry save tokens round-trip for all registered nodes", "[registry][serialization]")
{
    const NodeRegistry& registry = NodeRegistry::Get();
    const auto& all = registry.All();

    REQUIRE_FALSE(all.empty());

    for (const auto& [type, descriptor] : all)
    {
        REQUIRE_FALSE(descriptor.saveToken.empty());
        REQUIRE(registry.FindByToken(descriptor.saveToken) == type);
    }
}
