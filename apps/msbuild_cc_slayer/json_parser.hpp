#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace vscc
{
struct JsonValue
{
    using Object = std::map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;

    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;

    const Object* object() const;
    const Array* array() const;
    const std::string* string() const;
    bool boolean_or_false() const;
};

JsonValue parse_json_output(const std::string& output);
const JsonValue* object_member(const JsonValue::Object& object, const std::string& key);
std::string json_value_to_string(const JsonValue& value);
} // namespace vscc
