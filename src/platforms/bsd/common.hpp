#pragma once

#include <initializer_list>
#include <string>

namespace bsd_common {

std::string trim(std::string s);
std::string to_lower(std::string s);
bool contains_any(const std::string& haystack, std::initializer_list<const char*> needles);

} // namespace bsd_common
