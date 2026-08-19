#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace lightsurgeon {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool v) : type_(Type::Bool), bool_(v) {}
    Json(int v) : type_(Type::Number), number_(static_cast<double>(v)) {}
    Json(double v) : type_(Type::Number), number_(v) {}
    Json(const char* v) : type_(Type::String), string_(v ? v : "") {}
    Json(std::string v) : type_(Type::String), string_(std::move(v)) {}

    static Json array() {
        Json j;
        j.type_ = Type::Array;
        return j;
    }
    static Json object() {
        Json j;
        j.type_ = Type::Object;
        return j;
    }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool() const {
        require(Type::Bool);
        return bool_;
    }
    double asNumber() const {
        require(Type::Number);
        return number_;
    }
    const std::string& asString() const {
        require(Type::String);
        return string_;
    }
    const std::vector<Json>& asArray() const {
        require(Type::Array);
        return array_;
    }
    const std::map<std::string, Json>& asObject() const {
        require(Type::Object);
        return object_;
    }

    void push(Json v) {
        require(Type::Array);
        array_.push_back(std::move(v));
    }

    Json& operator[](const std::string& key) {
        if (type_ == Type::Null) {
            type_ = Type::Object;
        }
        require(Type::Object);
        return object_[key];
    }
    const Json& at(const std::string& key) const {
        require(Type::Object);
        auto it = object_.find(key);
        if (it == object_.end()) {
            throw std::runtime_error("JSON missing key: " + key);
        }
        return it->second;
    }
    bool has(const std::string& key) const {
        return type_ == Type::Object && object_.count(key) != 0;
    }
    const Json& get(const std::string& key, const Json& fallback) const {
        if (!has(key)) {
            return fallback;
        }
        return at(key);
    }

    std::string dump(int indent = 0) const;
    static Json parse(const std::string& text);

private:
    void require(Type t) const {
        if (type_ != t) {
            throw std::runtime_error("JSON type mismatch");
        }
    }

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    std::map<std::string, Json> object_;
};

}  // namespace lightsurgeon
