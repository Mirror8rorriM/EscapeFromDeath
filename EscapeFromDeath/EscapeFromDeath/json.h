#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace efd {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    Json();
    Json(std::nullptr_t);
    Json(bool v);
    Json(double v);
    Json(std::string v);
    Json(Array v);
    Json(Object v);

    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    int asInt(int fallback = 0) const;
    const std::string& asString() const;
    std::string asString(const std::string& fallback) const;
    const Array& asArray() const;
    const Object& asObject() const;
    bool contains(const std::string& key) const;
    const Json& at(const std::string& key) const;
    const Json& operator[](const std::string& key) const;

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_;
};

class JsonParser {
public:
    explicit JsonParser(std::string text);

    Json parse();
    static Json parseFile(const std::string& path);

private:
    Json parseValue();
    Json parseObject();
    Json parseArray();
    std::string parseString();
    double parseNumber();
    Json parseLiteral(const std::string& literal, Json value);
    void skipWhitespace();
    bool peek(char c) const;
    void expect(char c);

    std::string text_;
    std::size_t pos_ = 0;
};

} // namespace efd
