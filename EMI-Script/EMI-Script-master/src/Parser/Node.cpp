#include "Node.h"
#include "TypeConverters.h"

#ifdef DEBUG
constexpr const char* print_first = "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80";
constexpr const char* print_last  = "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80";
constexpr const char* print_add   = "\xE2\x94\x82   ";

void Node::print(const std::string& prefix, bool isLast)
{
	gCompileLogger() << prefix;
	gCompileLogger() << (isLast ? print_last : print_first);
	gCompileLogger() << TokensToName[type] << ": " << VariantToStr(this) << "\n";

	for (auto& node : children) {
		node->print(prefix + (isLast ? "    " : print_add), node == children.back());
	}
}
#endif

std::string VariantToStr(Node* n) {
	switch (n->data.index())
	{
	case 0: return std::get<std::string>(n->data);
	case 1: return FloatToStr(std::get<double>(n->data));
	case 2: return BoolToStr(std::get<bool>(n->data));
	default:
		return "";
		break;
	}
}

int VariantToInt(Node* n) {
	switch (n->data.index())
	{
	case 0: return StrToInt(std::get<std::string>(n->data).c_str());
	case 1: return FloatToInt(std::get<double>(n->data));
	case 2: return BoolToInt(std::get<bool>(n->data));
	default:
		return 0;
		break;
	}
}

double VariantToFloat(Node* n) {
	switch (n->data.index())
	{
	case 0: return StrToFloat(std::get<std::string>(n->data).c_str());
	case 1: return (std::get<double>(n->data));
	case 2: return BoolToFloat(std::get<bool>(n->data));
	default:
		return 0.f;
		break;
	}
}

bool VariantToBool(Node* n) {
	switch (n->data.index())
	{
	case 0: return StrToBool(std::get<std::string>(n->data).c_str());
	case 1: return FloatToBool(std::get<double>(n->data));
	case 2: return (std::get<bool>(n->data));
	default:
		return false;
		break;
	}
}
