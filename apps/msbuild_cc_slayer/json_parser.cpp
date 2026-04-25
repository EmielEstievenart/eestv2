#include "json_parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace vscc
{
const JsonValue::Object* JsonValue::object() const
{
    return std::get_if<Object>(&value);
}

const JsonValue::Array* JsonValue::array() const
{
    return std::get_if<Array>(&value);
}

const std::string* JsonValue::string() const
{
    return std::get_if<std::string>(&value);
}

bool JsonValue::boolean_or_false() const
{
    const auto* b = std::get_if<bool>(&value);
    return b != nullptr && *b;
}

class JsonParser
{
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    JsonValue parse()
    {
        skip_ws();
        JsonValue value = parse_value();
        skip_ws();
        if (pos_ != text_.size())
        {
            throw std::runtime_error("unexpected trailing JSON content");
        }
        return value;
    }

private:
    JsonValue parse_value()
    {
        skip_ws();
        if (pos_ >= text_.size())
        {
            throw std::runtime_error("unexpected end of JSON");
        }
        const char c = text_[pos_];
        if (c == '{')
        {
            return JsonValue{parse_object()};
        }
        if (c == '[')
        {
            return JsonValue{parse_array()};
        }
        if (c == '"')
        {
            return JsonValue{parse_string()};
        }
        if (starts_with("true"))
        {
            pos_ += 4;
            return JsonValue{true};
        }
        if (starts_with("false"))
        {
            pos_ += 5;
            return JsonValue{false};
        }
        if (starts_with("null"))
        {
            pos_ += 4;
            return JsonValue{nullptr};
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
        {
            return JsonValue{parse_number()};
        }
        throw std::runtime_error("invalid JSON value");
    }

    JsonValue::Object parse_object()
    {
        expect('{');
        JsonValue::Object object;
        skip_ws();
        if (consume('}'))
        {
            return object;
        }
        while (true)
        {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            object.emplace(std::move(key), parse_value());
            skip_ws();
            if (consume('}'))
            {
                return object;
            }
            expect(',');
        }
    }

    JsonValue::Array parse_array()
    {
        expect('[');
        JsonValue::Array array;
        skip_ws();
        if (consume(']'))
        {
            return array;
        }
        while (true)
        {
            array.push_back(parse_value());
            skip_ws();
            if (consume(']'))
            {
                return array;
            }
            expect(',');
        }
    }

    std::string parse_string()
    {
        expect('"');
        std::string result;
        while (pos_ < text_.size())
        {
            const char c = text_[pos_++];
            if (c == '"')
            {
                return result;
            }
            if (c != '\\')
            {
                result.push_back(c);
                continue;
            }
            if (pos_ >= text_.size())
            {
                throw std::runtime_error("unterminated JSON escape");
            }
            const char escaped = text_[pos_++];
            switch (escaped)
            {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u':
                append_unicode_escape(result);
                break;
            default: throw std::runtime_error("invalid JSON escape");
            }
        }
        throw std::runtime_error("unterminated JSON string");
    }

    int hex_value(char c) const
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        return -1;
    }

    unsigned read_unicode_quad()
    {
        if (pos_ + 4 > text_.size())
        {
            throw std::runtime_error("invalid unicode JSON escape");
        }

        unsigned value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const int digit = hex_value(text_[pos_++]);
            if (digit < 0)
            {
                throw std::runtime_error("invalid unicode JSON escape");
            }
            value = (value << 4) | static_cast<unsigned>(digit);
        }
        return value;
    }

    void append_utf8(std::string& result, unsigned code_point)
    {
        if (code_point <= 0x7f)
        {
            result.push_back(static_cast<char>(code_point));
        }
        else if (code_point <= 0x7ff)
        {
            result.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        else if (code_point <= 0xffff)
        {
            result.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        else if (code_point <= 0x10ffff)
        {
            result.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        else
        {
            throw std::runtime_error("invalid unicode JSON escape");
        }
    }

    void append_unicode_escape(std::string& result)
    {
        unsigned code_point = read_unicode_quad();

        if (code_point >= 0xd800 && code_point <= 0xdbff)
        {
            if (pos_ + 6 > text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u')
            {
                throw std::runtime_error("invalid unicode surrogate pair");
            }
            pos_ += 2;
            const unsigned low_surrogate = read_unicode_quad();
            if (low_surrogate < 0xdc00 || low_surrogate > 0xdfff)
            {
                throw std::runtime_error("invalid unicode surrogate pair");
            }
            code_point = 0x10000 + (((code_point - 0xd800) << 10) | (low_surrogate - 0xdc00));
        }
        else if (code_point >= 0xdc00 && code_point <= 0xdfff)
        {
            throw std::runtime_error("invalid unicode surrogate pair");
        }

        append_utf8(result, code_point);
    }

    double parse_number()
    {
        const std::size_t start = pos_;
        if (text_[pos_] == '-')
        {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
        {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.')
        {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
            {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
        {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
            {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
            {
                ++pos_;
            }
        }
        return std::stod(text_.substr(start, pos_ - start));
    }

    void skip_ws()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])))
        {
            ++pos_;
        }
    }

    bool starts_with(const char* literal) const
    {
        const std::string_view view(literal);
        return text_.compare(pos_, view.size(), view) == 0;
    }

    bool consume(char expected)
    {
        if (pos_ < text_.size() && text_[pos_] == expected)
        {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char expected)
    {
        if (!consume(expected))
        {
            std::ostringstream stream;
            stream << "expected '" << expected << "' at JSON offset " << pos_;
            throw std::runtime_error(stream.str());
        }
    }

    std::string text_;
    std::size_t pos_ = 0;
};

JsonValue parse_json_output(const std::string& output)
{
    const std::size_t begin = output.find_first_of("{[");
    const std::size_t end = output.find_last_of("}]");
    if (begin == std::string::npos || end == std::string::npos || begin > end)
    {
        throw std::runtime_error("MSBuild did not emit JSON");
    }
    return JsonParser(output.substr(begin, end - begin + 1)).parse();
}

const JsonValue* object_member(const JsonValue::Object& object, const std::string& key)
{
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::string json_value_to_string(const JsonValue& value)
{
    if (const auto* string = value.string())
    {
        return *string;
    }
    if (std::get_if<bool>(&value.value) != nullptr)
    {
        return value.boolean_or_false() ? "true" : "false";
    }
    return {};
}
} // namespace vscc
