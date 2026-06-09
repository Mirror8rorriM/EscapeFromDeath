#include "json.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace efd {

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
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open JSON file: " + path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
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
                    // Минимальная поддержка: сохраняем escape как текст.
                    if (pos_ + 4 > text_.size()) throw std::runtime_error("Bad unicode escape");
                    out += "\\u";
                    out.append(text_, pos_, 4);
                    pos_ += 4;
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
