#include "utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace efd {

std::string trim(const std::string& s) {
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch); });
    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end) return "";
    return std::string(begin, end);
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

int askInt(const std::string& prompt, int minValue, int maxValue) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n¬вод закрыт.\n";
            std::exit(0);
        }
        std::stringstream ss(line);
        int value = 0;
        if (ss >> value && value >= minValue && value <= maxValue) {
            return value;
        }
        std::cout << "¬ведите число от " << minValue << " до " << maxValue << ".\n";
    }
}

bool askYesNo(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (д/н): ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n¬вод закрыт.\n";
            std::exit(0);
        }
        line = toLowerAscii(trim(line));
        if (line == "y" || line == "yes" || line == "д" || line == "да") return true;
        if (line == "n" || line == "no" || line == "н" || line == "нет") return false;
        std::cout << "ќтветьте y/n.\n";
    }
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        item = trim(item);
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

std::string join(const std::set<std::string>& values, const std::string& delimiter) {
    std::string out;
    bool first = true;
    for (const auto& value : values) {
        if (!first) out += delimiter;
        out += value;
        first = false;
    }
    return out;
}

} 
