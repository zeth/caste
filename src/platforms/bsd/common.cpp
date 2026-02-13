#include "common.hpp"

#include <cctype>

namespace bsd_common {

std::string trim(std::string s) {
    auto notspace = [](unsigned char c){ return c != ' ' && c != '\t' && c != '\n' && c != '\r'; };
    while (!s.empty() && !notspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && !notspace((unsigned char)s.back())) s.pop_back();
    return s;
}

std::string to_lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool contains_any(const std::string& haystack, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (haystack.find(needle) != std::string::npos) return true;
    }
    return false;
}

} // namespace bsd_common
