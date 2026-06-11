#include "utils.h"

#include "console_encoding.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace efd {

std::string trim(const std::string& s) {
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch); });
    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end) return "";
    return std::string(begin, end);
}

namespace {

void appendUtf8(std::string& out, unsigned int cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool readCodePoint(const std::string& text, std::size_t& i, unsigned int& cp) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c < 0x80) {
        cp = c;
        ++i;
        return true;
    }
    if ((c >> 5) == 0x6 && i + 1 < text.size()) {
        cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
        i += 2;
        return true;
    }
    if ((c >> 4) == 0xE && i + 2 < text.size()) {
        cp = ((c & 0x0F) << 12)
           | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6)
           | (static_cast<unsigned char>(text[i + 2]) & 0x3F);
        i += 3;
        return true;
    }
    if ((c >> 3) == 0x1E && i + 3 < text.size()) {
        cp = ((c & 0x07) << 18)
           | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12)
           | ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6)
           | (static_cast<unsigned char>(text[i + 3]) & 0x3F);
        i += 4;
        return true;
    }

    cp = c;
    ++i;
    return false;
}

} // namespace

std::string toLowerUtf8(std::string value) {
    std::string out;
    out.reserve(value.size());

    std::size_t i = 0;
    while (i < value.size()) {
        unsigned int cp = 0;
        readCodePoint(value, i, cp);

        if (cp >= 'A' && cp <= 'Z') {
            cp += 'a' - 'A';
        } else if (cp >= 0x0410 && cp <= 0x042F) { // А-Я
            cp += 0x20;
        } else if (cp == 0x0401) { // Ё
            cp = 0x0451;
        }

        appendUtf8(out, cp);
    }

    return out;
}

std::string toLowerAscii(std::string value) {
    return toLowerUtf8(std::move(value));
}

int askInt(const std::string& prompt, int minValue, int maxValue) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!readLineUtf8(line)) {
            std::cout << "\nВвод закрыт.\n";
            std::exit(0);
        }
        std::stringstream ss(line);
        int value = 0;
        if (ss >> value && value >= minValue && value <= maxValue) {
            return value;
        }
        std::cout << "Введите число от " << minValue << " до " << maxValue << ".\n";
    }
}

bool askYesNo(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (д/н, y/n): ";
        std::string line;
        if (!readLineUtf8(line)) {
            std::cout << "\nВвод закрыт.\n";
            std::exit(0);
        }
        line = toLowerAscii(trim(line));
        if (line == "y" || line == "yes" || line == "д" || line == "да") return true;
        if (line == "n" || line == "no" || line == "н" || line == "нет") return false;
        std::cout << "Ответьте д/н или y/n.\n";
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

} // namespace efd
