#include "lightsurgeon/json.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lightsurgeon {
namespace {

class Parser {
public:
    explicit Parser(const std::string& t) : text_(t) {}

    Json parseValue() {
        skip();
        if (pos_ >= text_.size()) {
            throw std::runtime_error("Unexpected end of JSON");
        }
        const char c = text_[pos_];
        if (c == 'n') return parseLiteral("null", Json());
        if (c == 't') return parseLiteral("true", Json(true));
        if (c == 'f') return parseLiteral("false", Json(false));
        if (c == '"') return Json(parseString());
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return Json(parseNumber());
        throw std::runtime_error(std::string("Unexpected JSON char: ") + c);
    }

    void finish() {
        skip();
        if (pos_ != text_.size()) {
            throw std::runtime_error("Trailing JSON content");
        }
    }

private:
    void skip() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    Json parseLiteral(const char* lit, Json value) {
        const std::size_t n = std::char_traits<char>::length(lit);
        if (text_.compare(pos_, n, lit) != 0) {
            throw std::runtime_error("Invalid JSON literal");
        }
        pos_ += n;
        return value;
    }

    std::string parseString() {
        ++pos_;
        std::string out;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') {
                return out;
            }
            if (c == '\\') {
                if (pos_ >= text_.size()) {
                    throw std::runtime_error("Unterminated string escape");
                }
                const char e = text_[pos_++];
                switch (e) {
                    case '"':
                    case '\\':
                    case '/':
                        out.push_back(e);
                        break;
                    case 'b':
                        out.push_back('\b');
                        break;
                    case 'f':
                        out.push_back('\f');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) {
                            throw std::runtime_error("Invalid unicode escape");
                        }
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = text_[pos_++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                            else throw std::runtime_error("Invalid hex in unicode escape");
                        }
                        if (code <= 0x7F) {
                            out.push_back(static_cast<char>(code));
                        } else if (code <= 0x7FF) {
                            out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
                            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default:
                        throw std::runtime_error("Unknown string escape");
                }
            } else {
                out.push_back(c);
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    double parseNumber() {
        const std::size_t start = pos_;
        if (text_[pos_] == '-') ++pos_;
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            throw std::runtime_error("Invalid number");
        }
        if (text_[pos_] == '0') {
            ++pos_;
        } else {
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                throw std::runtime_error("Invalid fraction");
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                throw std::runtime_error("Invalid exponent");
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        return std::strtod(text_.c_str() + start, nullptr);
    }

    Json parseArray() {
        ++pos_;
        Json arr = Json::array();
        skip();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return arr;
        }
        while (true) {
            arr.push(parseValue());
            skip();
            if (pos_ >= text_.size()) throw std::runtime_error("Unterminated array");
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == ']') {
                ++pos_;
                return arr;
            }
            throw std::runtime_error("Expected comma or ]");
        }
    }

    Json parseObject() {
        ++pos_;
        Json obj = Json::object();
        skip();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return obj;
        }
        while (true) {
            skip();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                throw std::runtime_error("Expected object key");
            }
            std::string key = parseString();
            skip();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                throw std::runtime_error("Expected colon");
            }
            ++pos_;
            obj[key] = parseValue();
            skip();
            if (pos_ >= text_.size()) throw std::runtime_error("Unterminated object");
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == '}') {
                ++pos_;
                return obj;
            }
            throw std::runtime_error("Expected comma or }");
        }
    }

    const std::string& text_;
    std::size_t pos_ = 0;
};

void dumpValue(std::ostringstream& os, const Json& v, int indent, int level) {
    auto nl = [&]() {
        if (indent <= 0) return;
        os << '\n' << std::string(static_cast<std::size_t>(indent * level), ' ');
    };
    switch (v.type()) {
        case Json::Type::Null:
            os << "null";
            break;
        case Json::Type::Bool:
            os << (v.asBool() ? "true" : "false");
            break;
        case Json::Type::Number: {
            std::ostringstream n;
            n.precision(17);
            n << v.asNumber();
            os << n.str();
            break;
        }
        case Json::Type::String: {
            os << '"';
            for (unsigned char c : v.asString()) {
                switch (c) {
                    case '"':
                        os << "\\\"";
                        break;
                    case '\\':
                        os << "\\\\";
                        break;
                    case '\n':
                        os << "\\n";
                        break;
                    case '\r':
                        os << "\\r";
                        break;
                    case '\t':
                        os << "\\t";
                        break;
                    default:
                        if (c < 0x20) {
                            os << "\\u00";
                            const char* hex = "0123456789abcdef";
                            os << hex[c >> 4] << hex[c & 0xF];
                        } else {
                            os << static_cast<char>(c);
                        }
                }
            }
            os << '"';
            break;
        }
        case Json::Type::Array: {
            const auto& a = v.asArray();
            os << '[';
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (indent > 0) nl();
                dumpValue(os, a[i], indent, level + 1);
                if (i + 1 < a.size()) os << ',';
            }
            if (indent > 0 && !a.empty()) {
                --level;
                nl();
            }
            os << ']';
            break;
        }
        case Json::Type::Object: {
            const auto& o = v.asObject();
            os << '{';
            std::size_t i = 0;
            for (const auto& kv : o) {
                if (indent > 0) nl();
                Json key(kv.first);
                dumpValue(os, key, 0, 0);
                os << (indent > 0 ? ": " : ":");
                dumpValue(os, kv.second, indent, level + 1);
                if (i + 1 < o.size()) os << ',';
                ++i;
            }
            if (indent > 0 && !o.empty()) {
                --level;
                nl();
            }
            os << '}';
            break;
        }
    }
}

}  // namespace

std::string Json::dump(int indent) const {
    std::ostringstream os;
    dumpValue(os, *this, indent, 1);
    return os.str();
}

Json Json::parse(const std::string& text) {
    Parser p(text);
    Json v = p.parseValue();
    p.finish();
    return v;
}

}  // namespace lightsurgeon
