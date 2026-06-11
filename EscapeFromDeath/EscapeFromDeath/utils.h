#pragma once

#include <set>
#include <string>
#include <vector>

namespace efd {

std::string trim(const std::string& s);
std::string toLowerAscii(std::string value);
std::string toLowerUtf8(std::string value);
int askInt(const std::string& prompt, int minValue, int maxValue);
bool askYesNo(const std::string& prompt);
std::vector<std::string> split(const std::string& text, char delimiter);
std::string join(const std::set<std::string>& values, const std::string& delimiter);

} // namespace efd
