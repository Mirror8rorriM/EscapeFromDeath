#include "json.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace efd {

namespace {

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

unsigned int readHexCodePoint(const std::string& text, std::size_t& pos) {
    if (pos + 4 > text.size()) throw std::runtime_error("Bad unicode escape");

    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
        int digit = hexValue(text[pos++]);
        if (digit < 0) throw std::runtime_error("Bad unicode escape");
        value = (value << 4) | static_cast<unsigned int>(digit);
    }
    return value;
}

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
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        throw std::runtime_error("Bad unicode escape");
    }
}

} 

Json::Json() : value_(nullptr) {}

Json::Json(std::nullptr_t) : value_(nullptr) {}

Json::Json(bool v) : value_(v) {}

Json::Json(double v) : value_(v) {}

Json::Json(std::string v) : value_(std::move(v)) {}

Json::Json(Array v) : value_(std::move(v)) {}

Json::Json(Object v) : value_(std::move(v)) {}

bool Json::isNull() const {
    return std::holds_alternative<std::nullptr_t>(value_);
}

bool Json::isBool() const {
    return std::holds_alternative<bool>(value_);
}

bool Json::isNumber() const {
    return std::holds_alternative<double>(value_);
}

bool Json::isString() const {
    return std::holds_alternative<std::string>(value_);
}

bool Json::isArray() const {
    return std::holds_alternative<Array>(value_);
}

bool Json::isObject() const {
    return std::holds_alternative<Object>(value_);
}

bool Json::asBool(bool fallback) const {
    return isBool() ? std::get<bool>(value_) : fallback;
}

double Json::asNumber(double fallback) const {
    return isNumber() ? std::get<double>(value_) : fallback;
}

int Json::asInt(int fallback) const {
    return static_cast<int>(asNumber(fallback));
}

const std::string& Json::asString() const {
    if (!isString()) throw std::runtime_error("Json value is not a string");
    return std::get<std::string>(value_);
}

std::string Json::asString(const std::string& fallback) const {
    return isString() ? std::get<std::string>(value_) : fallback;
}

const Json::Array& Json::asArray() const {
    if (!isArray()) throw std::runtime_error("Json value is not an array");
    return std::get<Array>(value_);
}

const Json::Object& Json::asObject() const {
    if (!isObject()) throw std::runtime_error("Json value is not an object");
    return std::get<Object>(value_);
}

bool Json::contains(const std::string& key) const {
    if (!isObject()) return false;
    const auto& obj = std::get<Object>(value_);
    return obj.find(key) != obj.end();
}

const Json& Json::at(const std::string& key) const {
    const auto& obj = asObject();
    auto it = obj.find(key);
    if (it == obj.end()) throw std::runtime_error("Json key not found: " + key);
    return it->second;
}

const Json& Json::operator[](const std::string& key) const {
    return at(key);
}

JsonParser::JsonParser(std::string text) : text_(std::move(text)) {}

Json JsonParser::parse() {
    skipWhitespace();
    Json result = parseValue();
    skipWhitespace();
    if (pos_ != text_.size()) {
        throw std::runtime_error("Unexpected characters after JSON document");
    }
    return result;
}

Json JsonParser::parseFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open JSON file: " + path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return JsonParser(text).parse();
}

Json JsonParser::parseValue() {
    skipWhitespace();
    if (pos_ >= text_.size()) throw std::runtime_error("Unexpected end of JSON");

    char c = text_[pos_];
    if (c == '"') return Json(parseString());
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == 't') return parseLiteral("true", Json(true));
    if (c == 'f') return parseLiteral("false", Json(false));
    if (c == 'n') return parseLiteral("null", Json(nullptr));
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return Json(parseNumber());

    throw std::runtime_error(std::string("Unexpected JSON token: ") + c);
}

Json JsonParser::parseObject() {
    expect('{');
    Json::Object obj;
    skipWhitespace();
    if (peek('}')) {
        expect('}');
        return Json(std::move(obj));
    }

    while (true) {
        skipWhitespace();
        std::string key = parseString();
        skipWhitespace();
        expect(':');
        obj[key] = parseValue();
        skipWhitespace();
        if (peek('}')) {
            expect('}');
            break;
        }
        expect(',');
    }
    return Json(std::move(obj));
}

Json JsonParser::parseArray() {
    expect('[');
    Json::Array arr;
    skipWhitespace();
    if (peek(']')) {
        expect(']');
        return Json(std::move(arr));
    }

    while (true) {
        arr.push_back(parseValue());
        skipWhitespace();
        if (peek(']')) {
            expect(']');
            break;
        }
        expect(',');
    }
    return Json(std::move(arr));
}

std::string JsonParser::parseString() {
    expect('"');
    std::string out;
    while (pos_ < text_.size()) {
        char c = text_[pos_++];
        if (c == '"') return out;
        if (c == '\\') {
            if (pos_ >= text_.size()) throw std::runtime_error("Bad JSON escape");
            char e = text_[pos_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned int cp = readHexCodePoint(text_, pos_);
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos_ + 6 > text_.size() || text_[pos_++] != '\\' || text_[pos_++] != 'u') {
                            throw std::runtime_error("Bad unicode surrogate pair");
                        }
                        unsigned int low = readHexCodePoint(text_, pos_);
                        if (low < 0xDC00 || low > 0xDFFF) {
                            throw std::runtime_error("Bad unicode surrogate pair");
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        throw std::runtime_error("Bad unicode surrogate pair");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: throw std::runtime_error("Unknown JSON escape");
            }
        } else {
            out.push_back(c);
        }
    }
    throw std::runtime_error("Unterminated JSON string");
}

double JsonParser::parseNumber() {
    std::size_t start = pos_;
    if (text_[pos_] == '-') ++pos_;
    while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    if (pos_ < text_.size() && text_[pos_] == '.') {
        ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
        ++pos_;
        if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }
    return std::strtod(text_.c_str() + start, nullptr);
}

Json JsonParser::parseLiteral(const std::string& literal, Json value) {
    if (text_.compare(pos_, literal.size(), literal) != 0) {
        throw std::runtime_error("Bad JSON literal");
    }
    pos_ += literal.size();
    return value;
}

void JsonParser::skipWhitespace() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
}

bool JsonParser::peek(char c) const {
    return pos_ < text_.size() && text_[pos_] == c;
}

void JsonParser::expect(char c) {
    if (pos_ >= text_.size() || text_[pos_] != c) {
        throw std::runtime_error(std::string("Expected '") + c + "'");
    }
    ++pos_;
}

} 
