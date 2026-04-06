#pragma once

#include "../../compiler/graphCompiler.h"
#include "../../graph/nodeField.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace
{
// Small helper builders shared by node compile callbacks.
inline Node* MakeNode(Token t)
{
    Node* node = new Node();
    node->type = t;
    return node;
}

inline Node* MakeIdNode(const std::string& name)
{
    Node* node = new Node();
    node->type = Token::Id;
    node->data = name;
    return node;
}

inline const Pin* FindInputPin(const VisualNode& n, const char* name)
{
    for (const Pin& p : n.inPins)
        if (p.name == name)
            return &p;
    return nullptr;
}

inline const Pin* FindOutputPin(const VisualNode& n, const char* name)
{
    for (const Pin& p : n.outPins)
        if (p.name == name)
            return &p;
    return nullptr;
}

inline const std::string* FindField(const VisualNode& n, const char* name)
{
    for (const NodeField& f : n.fields)
        if (f.name == name)
            return &f.value;
    return nullptr;
}

inline const std::string* FindInputFallbackField(const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (fieldName && fieldName[0] != '\0')
        return FindField(n, fieldName);

    return FindField(n, pin.name.c_str());
}

inline bool PopulateExactPinsAndFields(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    // Standard load path: pin id count must exactly match descriptor pin count.
    if (pinIds.size() != desc.pins.size())
        return false;

    size_t pinIndex = 0;
    for (const PinDescriptor& pd : desc.pins)
    {
        const uint32_t pinId = static_cast<uint32_t>(pinIds[pinIndex++]);
        Pin p = MakePin(pinId, n.id, desc.type, pd.name, pd.type, pd.isInput, pd.isMultiInput);
        if (pd.isInput)
            n.inPins.push_back(p);
        else
            n.outPins.push_back(p);
    }

    for (const FieldDescriptor& fd : desc.fields)
        n.fields.push_back(MakeNodeField(fd));

    return true;
}

inline Node* MakeNumberLiteral(double value)
{
    Node* node = new Node();
    node->type = Token::Number;
    node->data = value;
    return node;
}

inline Node* MakeBoolLiteral(bool value)
{
    Node* node = new Node();
    node->type = value ? Token::True : Token::False;
    node->data = value;
    return node;
}

inline Node* MakeStringLiteral(const std::string& value)
{
    Node* node = new Node();
    node->type = Token::Literal;
    node->data = value;
    return node;
}

inline Token ParseOperatorToken(const std::string* opStr)
{
    if (!opStr || *opStr == "+") return Token::Add;
    if (*opStr == "-") return Token::Sub;
    if (*opStr == "*") return Token::Mult;
    if (*opStr == "/") return Token::Div;
    return Token::None;
}

inline Token ParseComparisonToken(const std::string* opStr)
{
    if (!opStr || *opStr == "==") return Token::Equal;
    if (*opStr == "!=") return Token::NotEqual;
    if (*opStr == "<") return Token::Less;
    if (*opStr == "<=") return Token::LessEqual;
    if (*opStr == ">") return Token::Larger;
    if (*opStr == ">=") return Token::LargerEqual;
    return Token::None;
}

inline Token ParseLogicToken(const std::string* opStr)
{
    if (!opStr || *opStr == "AND") return Token::And;
    if (*opStr == "OR") return Token::Or;
    return Token::None;
}

inline PinType VariableTypeFromString(const std::string& typeName)
{
    if (typeName == "Boolean") return PinType::Boolean;
    if (typeName == "String")  return PinType::String;
    if (typeName == "Array")   return PinType::Array;
    return PinType::Number;
}

inline Node* BuildNumberOperand(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    try { return MakeNumberLiteral(fieldValue ? std::stod(*fieldValue) : 0.0); }
    catch (...) { return MakeNumberLiteral(0.0); }
}

inline Node* BuildBoolOperand(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    const std::string value = fieldValue ? *fieldValue : "false";
    return MakeBoolLiteral(value == "true" || value == "True" || value == "1");
}

inline Node* BuildArrayInput(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    return compiler->BuildArrayLiteralNode(fieldValue ? *fieldValue : "[]");
}

inline Node* BuildNumberInput(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    double parsed = 0.0;
    if (fieldValue)
    {
        try { parsed = std::stod(*fieldValue); }
        catch (...) { parsed = 0.0; }
    }

    return MakeNumberLiteral(parsed);
}

inline Node* BuildTypedFieldValue(GraphCompiler* compiler, const VisualNode& n, const char* typeFieldName, const char* valueFieldName)
{
    // Convert a Type + Value field pair into an AST literal node.
    const std::string* typeText = FindField(n, typeFieldName);
    const std::string* valueText = FindField(n, valueFieldName);

    const std::string typeName = typeText ? *typeText : "Number";
    const std::string valueName = valueText ? *valueText : "0.0";

    if (typeName == "Boolean")
        return MakeBoolLiteral(valueName == "true" || valueName == "True" || valueName == "1");

    if (typeName == "String")
        return MakeStringLiteral(valueName);

    if (typeName == "Array")
        return compiler->BuildArrayLiteralNode(valueName);

    try { return MakeNumberLiteral(std::stod(valueName)); }
    catch (...) { return MakeNumberLiteral(0.0); }
}

inline Node* BuildOutputValue(GraphCompiler* compiler, const VisualNode& n, const Pin& valuePin)
{
    // Build output value from link when present; otherwise fall back to local/default source.
    if (compiler->Resolve(valuePin))
        return compiler->BuildExpr(valuePin);

    const Pin* flowIn = FindInputPin(n, "In");
    if (!flowIn)
        flowIn = FindInputPin(n, "Flow");

    if (flowIn)
    {
        const FlowTarget* flowSrc = compiler->ResolveFlow(*flowIn);
        if (flowSrc && flowSrc->node && flowSrc->node->nodeType == NodeType::ForEach)
        {
            int elementOutIdx = -1;
            for (int i = 0; i < static_cast<int>(flowSrc->node->outPins.size()); ++i)
            {
                const Pin& outPin = flowSrc->node->outPins[static_cast<size_t>(i)];
                if (outPin.type != PinType::Flow && outPin.name == "Element")
                {
                    elementOutIdx = i;
                    break;
                }
            }

            if (elementOutIdx >= 0)
                return compiler->BuildNode(*flowSrc->node, elementOutIdx);
        }
    }

    return compiler->BuildExpr(valuePin);
}

inline Node* ParseTextLiteralNode(const std::string& text)
{
    return MakeStringLiteral(text);
}

inline std::string NodeInstanceSuffix(const VisualNode& n)
{
    return std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

inline std::string LoopIndexVarNameForNode(const VisualNode& n) { return "__loop_i_" + NodeInstanceSuffix(n); }
inline std::string LoopLastIndexVarNameForNode(const VisualNode& n) { return "__loop_last_i_" + NodeInstanceSuffix(n); }
inline std::string LoopStartVarNameForNode(const VisualNode& n) { return "__loop_start_i_" + NodeInstanceSuffix(n); }
inline std::string LoopEndVarNameForNode(const VisualNode& n) { return "__loop_end_i_" + NodeInstanceSuffix(n); }
inline std::string ForEachElementVarNameForNode(const VisualNode& n) { return "__foreach_elem_" + NodeInstanceSuffix(n); }

inline std::string TrimCopy(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

inline bool TryParseDoubleExact(const std::string& s, double& out)
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

inline std::vector<std::string> SplitArrayItemsTopLevel(const std::string& text)
{
    std::vector<std::string> out;
    std::string current;
    current.reserve(text.size());

    int bracketDepth = 0;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inQuote = false;
    char quoteChar = '\0';
    bool escape = false;

    auto pushCurrent = [&]() {
        out.push_back(TrimCopy(current));
        current.clear();
    };

    for (char ch : text)
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
            {
                escape = true;
            }
            else if (ch == quoteChar)
            {
                inQuote = false;
            }
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

inline Node* BuildBranchNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* conditionPin = FindInputPin(n, "Condition");
    if (!conditionPin)
    {
        compiler->Error("Branch node needs Condition input");
        return nullptr;
    }

    Node* ifNode = MakeNode(Token::If);
    Node* condExpr = compiler->BuildExpr(*conditionPin);
    if (compiler->HasError) { delete ifNode; return nullptr; }

    // If the connected source pin carries a Number or Any value, wrap with != 0
    // so that native functions returning 1.0/0.0 work as boolean conditions.
    const PinSource* condSrc = compiler->Resolve(*conditionPin);
    if (condSrc && condSrc->node && condSrc->pinIdx >= 0 &&
        condSrc->pinIdx < static_cast<int>(condSrc->node->outPins.size()))
    {
        const PinType srcType = condSrc->node->outPins[static_cast<size_t>(condSrc->pinIdx)].type;
        if (srcType == PinType::Number || srcType == PinType::Any)
        {
            Node* notEq = MakeNode(Token::NotEqual);
            notEq->children.push_back(condExpr);
            notEq->children.push_back(MakeNumberLiteral(0.0));
            condExpr = notEq;
        }
    }

    ifNode->children.push_back(condExpr);

    ifNode->children.push_back(MakeNode(Token::Scope));

    if (FindOutputPin(n, "False"))
    {
        Node* elseScope = MakeNode(Token::Scope);
        Node* elseNode = MakeNode(Token::Else);
        elseNode->children.push_back(elseScope);
        ifNode->children.push_back(elseNode);
    }

    return ifNode;
}

inline Node* BuildLoopNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* startPin = FindInputPin(n, "Start");
    const Pin* countPin = FindInputPin(n, "Count");
    if (!countPin)
    {
        compiler->Error("Loop node needs Count input");
        return nullptr;
    }

    Node* startExpr = nullptr;
    if (startPin)
        startExpr = BuildNumberInput(compiler, n, *startPin);

    if (!startExpr)
        startExpr = MakeNumberLiteral(0.0);
    else if (startExpr->type == Token::Number)
    {
        if (double* v = std::get_if<double>(&startExpr->data))
            *v = std::round(*v);
    }

    Node* countExpr = BuildNumberInput(compiler, n, *countPin);
    if (countExpr && countExpr->type == Token::Number)
    {
        if (double* v = std::get_if<double>(&countExpr->data))
            *v = std::round(*v);
    }

    if (compiler->HasError)
    {
        delete startExpr;
        delete countExpr;
        return nullptr;
    }

    const std::string indexVarName = LoopIndexVarNameForNode(n);
    const std::string lastIndexVarName = LoopLastIndexVarNameForNode(n);
    const std::string startVarName = LoopStartVarNameForNode(n);
    const std::string endVarName = LoopEndVarNameForNode(n);

    Node* prelude = MakeNode(Token::Scope);

    Node* startDecl = MakeNode(Token::VarDeclare);
    startDecl->data = startVarName;
    startDecl->children.push_back(MakeNode(Token::TypeNumber));
    startDecl->children.push_back(startExpr);
    prelude->children.push_back(startDecl);

    Node* endDecl = MakeNode(Token::VarDeclare);
    endDecl->data = endVarName;
    endDecl->children.push_back(MakeNode(Token::TypeNumber));
    Node* endExpr = MakeNode(Token::Add);
    endExpr->children.push_back(MakeIdNode(startVarName));
    endExpr->children.push_back(countExpr);
    endDecl->children.push_back(endExpr);
    prelude->children.push_back(endDecl);

    Node* varDecl = MakeNode(Token::VarDeclare);
    varDecl->data = indexVarName;
    varDecl->children.push_back(MakeNode(Token::TypeNumber));
    varDecl->children.push_back(MakeIdNode(startVarName));

    Node* cond = MakeNode(Token::Less);
    cond->children.push_back(MakeIdNode(indexVarName));
    cond->children.push_back(MakeIdNode(endVarName));

    Node* incr = MakeNode(Token::Increment);
    incr->children.push_back(MakeIdNode(indexVarName));

    Node* body = MakeNode(Token::Scope);
    Node* cacheLastIndexAssign = MakeNode(Token::Assign);
    cacheLastIndexAssign->children.push_back(MakeIdNode(lastIndexVarName));
    cacheLastIndexAssign->children.push_back(MakeIdNode(indexVarName));
    body->children.push_back(cacheLastIndexAssign);

    Node* completed = MakeNode(Token::Scope);
    Node* forNode = MakeNode(Token::For);
    forNode->children.push_back(varDecl);
    forNode->children.push_back(cond);
    forNode->children.push_back(incr);
    forNode->children.push_back(body);
    forNode->children.push_back(completed);

    prelude->children.push_back(forNode);
    return prelude;
}

inline Node* BuildForEachNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    if (!arrayPin)
    {
        compiler->Error("For Each node needs Array input");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    if (compiler->HasError || !arrayExpr)
    {
        delete arrayExpr;
        return nullptr;
    }

    Node* loop = MakeNode(Token::For);
    loop->data = ForEachElementVarNameForNode(n);
    loop->children.push_back(arrayExpr);
    loop->children.push_back(MakeNode(Token::Scope));
    loop->children.push_back(MakeNode(Token::Scope));
    return loop;
}

inline Node* BuildWhileNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* conditionPin = FindInputPin(n, "Condition");
    if (!conditionPin)
    {
        compiler->Error("While node needs Condition input");
        return nullptr;
    }

    Node* condExpr = compiler->BuildExpr(*conditionPin);
    if (compiler->HasError || !condExpr)
        return nullptr;

    Node* whileNode = MakeNode(Token::While);
    whileNode->children.push_back(condExpr);
    whileNode->children.push_back(MakeNode(Token::Scope));
    return whileNode;
}

inline Node* BuildStructCreateNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* schemaText = FindField(n, "Schema Fields");
    std::string inner = schemaText ? TrimCopy(*schemaText) : "";
    if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']')
        inner = inner.substr(1, inner.size() - 2);
    const std::vector<std::string> defs = SplitArrayItemsTopLevel(inner);

    auto parseBool = [](const std::string& s) -> bool {
        return s == "true" || s == "True" || s == "1";
    };

    auto defaultForType = [&](PinType t) -> Node*
    {
        switch (t)
        {
            case PinType::Number:  return MakeNumberLiteral(0.0);
            case PinType::Boolean: return MakeBoolLiteral(false);
            case PinType::String:  return MakeStringLiteral("");
            case PinType::Array:   return compiler->BuildArrayLiteralNode("[]");
            default:               return MakeNode(Token::Null);
        }
    };

    auto buildTypedLiteral = [&](PinType t, const std::string* raw) -> Node*
    {
        if (!raw)
            return defaultForType(t);

        switch (t)
        {
            case PinType::Number:
            {
                double v = 0.0;
                if (TryParseDoubleExact(*raw, v))
                    return MakeNumberLiteral(v);
                return MakeNumberLiteral(0.0);
            }
            case PinType::Boolean:
                return MakeBoolLiteral(parseBool(*raw));
            case PinType::String:
                return MakeStringLiteral(*raw);
            case PinType::Array:
                return compiler->BuildArrayLiteralNode(*raw);
            default:
                return MakeStringLiteral(*raw);
        }
    };

    Node* arr = MakeNode(Token::Array);
    for (std::string raw : defs)
    {
        raw = TrimCopy(raw);
        if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') || (raw.front() == '\'' && raw.back() == '\'')))
            raw = raw.substr(1, raw.size() - 2);
        const size_t sep = raw.find(':');
        const std::string fieldName = (sep == std::string::npos) ? raw : TrimCopy(raw.substr(0, sep));
        const std::string typeName = (sep == std::string::npos) ? "Any" : TrimCopy(raw.substr(sep + 1));
        const PinType fieldType = VariableTypeFromString(typeName);

        if (const Pin* pin = FindInputPin(n, fieldName.c_str()))
        {
            if (const PinSource* src = compiler->Resolve(*pin))
            {
                arr->children.push_back(compiler->BuildNode(*src->node, src->pinIdx));
                if (compiler->HasError) { delete arr; return nullptr; }
                continue;
            }
        }

        const std::string* fieldValue = FindField(n, fieldName.c_str());
        arr->children.push_back(buildTypedLiteral(fieldType, fieldValue));
        if (compiler->HasError)
        {
            delete arr;
            return nullptr;
        }
    }
    return arr;
}
}
