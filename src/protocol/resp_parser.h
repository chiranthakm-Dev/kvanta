#pragma once

#include <vector>
#include <string>
#include <optional>

enum class RespType { SimpleString, Error, Integer, BulkString, Array };

struct RespValue {
    RespType type;
    std::string str;
    long long integer = 0;
    std::vector<RespValue> array;
};

class RespParser {
public:
    std::optional<RespValue> parse(const std::string& data);
private:
    std::optional<RespValue> parse_value(const std::string& data, size_t& pos);
};
