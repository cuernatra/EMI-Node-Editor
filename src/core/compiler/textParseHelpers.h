#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

/// Trim ASCII whitespace from both ends of a string.
///
/// @param s Input string.
/// @return Trimmed copy.
inline std::string TrimCopy(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

/// Parse a string as a double and require full consumption.
///
/// @param s Input string.
/// @param out Parsed value.
/// @return True if `s` is exactly a number.
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

/// Split an array-literal string into top-level comma-separated items.
///
/// Respects nesting (`[]`, `()`, `{}`) and quoted strings, so nested arrays or
/// strings containing commas are not split incorrectly.
///
/// @param text Inner array contents (e.g. `"1, [2,3], \"a,b\""`).
/// @return List of item strings (trimmed).
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
