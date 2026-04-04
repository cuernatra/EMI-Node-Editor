#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

namespace
{

    Node* CompileFunctionNode(GraphCompiler* compiler, const VisualNode& n)
    {
        const std::string* nameStr = FindField(n, "Name");
        const std::string funcName = (nameStr && !nameStr->empty())
            ? *nameStr : "__unnamed_func";

        Node* funcDecl = new Node();
        funcDecl->type = Token::FunctionDef;
        funcDecl->data = funcName;

        Node* params = new Node();
        params->type = Token::CallParams;
        for (int i = 0; ; ++i)
        {
            const std::string* p = FindField(n, ("Param" + std::to_string(i)).c_str());
            if (!p || p->empty()) break;

            Node* param = new Node();
            param->type = Token::Id;
            param->data = *p;
            Node* typeNode = new Node();
            typeNode->type = Token::AnyType;
            param->children.push_back(typeNode);
            params->children.push_back(param);
        }
        funcDecl->children.push_back(params);

        Node* body = new Node();
        body->type = Token::Scope;

        const Pin* bodyOut = FindOutputPin(n, "Body");
        if (bodyOut)
            compiler->AppendFlowFromPin(*bodyOut, body);

        if (compiler->HasError) { delete funcDecl; return nullptr; }

        // Automaattinen Return bodyn viimeisestä lausekkeesta
        if (!body->children.empty())
        {
            Node* lastStmt = body->children.back();
            body->children.pop_back();
            Node* ret = new Node();
            ret->type = Token::Return;
            ret->children.push_back(lastStmt);
            body->children.push_back(ret);
        }

        funcDecl->children.push_back(body);
        return funcDecl;
    }

    Node* CompileCallFunctionNode(GraphCompiler* compiler, const VisualNode& n)
    {
        const std::string* nameStr = FindField(n, "Name");
        const std::string funcName = (nameStr && !nameStr->empty())
            ? *nameStr : "__unnamed_func";

        std::vector<Node*> args;
        for (const Pin& pin : n.inPins)
        {
            if (pin.type == PinType::Flow) continue;
            Node* arg = compiler->BuildExpr(pin);
            if (compiler->HasError)
            {
                for (Node* a : args) delete a;
                return nullptr;
            }
            args.push_back(arg);
        }

        return compiler->EmitFunctionCall(funcName, std::move(args));
    }

} // namespace

void NodeRegistry::RegisterFunctionNodes()
{
    Register({
        NodeType::Function,
        "Function",
        {
            { "Body", PinType::Flow, false }
        },
        {
            { "Name", PinType::String, "myFunction" }
        },
        CompileFunctionNode,
        nullptr,
        "Functions",
        {},
        "Function",
        {},
        NodeRenderStyle::Function
        });

    Register({
        NodeType::CallFunction,
        "Call Function",
        {
            { "In",     PinType::Flow,   true  },
            { "Out",    PinType::Flow,   false },
            { "Result", PinType::Number, false }
        },
        {
            { "Name", PinType::String, "myFunction" }
        },
        CompileCallFunctionNode,
        nullptr,
        "Functions",
        {},
        "CallFunction",
        {},
        NodeRenderStyle::Function
        });
}
