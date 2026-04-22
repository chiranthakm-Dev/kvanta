#include "resp_parser.h"
#include <sstream>

std::optional<RespValue> RespParser::parse(const std::string& data) {
    size_t pos = 0;
    return parse_value(data, pos);
}

std::optional<RespValue> RespParser::parse_value(const std::string& data, size_t& pos) {
    if (pos >= data.size()) return std::nullopt;
    char type = data[pos++];
    switch (type) {
        case '+': {
            size_t end = data.find('\r', pos);
            if (end == std::string::npos || data[end+1] != '\n') return std::nullopt;
            std::string str = data.substr(pos, end - pos);
            pos = end + 2;
            return RespValue{RespType::SimpleString, str};
        }
        case '-': {
            size_t end = data.find('\r', pos);
            if (end == std::string::npos || data[end+1] != '\n') return std::nullopt;
            std::string str = data.substr(pos, end - pos);
            pos = end + 2;
            return RespValue{RespType::Error, str};
        }
        case ':': {
            size_t end = data.find('\r', pos);
            if (end == std::string::npos || data[end+1] != '\n') return std::nullopt;
            std::string num_str = data.substr(pos, end - pos);
            long long num = std::stoll(num_str);
            pos = end + 2;
            return RespValue{RespType::Integer, "", num};
        }
        case '$': {
            size_t end = data.find('\r', pos);
            if (end == std::string::npos || data[end+1] != '\n') return std::nullopt;
            std::string len_str = data.substr(pos, end - pos);
            long long len = std::stoll(len_str);
            pos = end + 2;
            if (len == -1) return RespValue{RespType::BulkString, ""};
            if (pos + len + 2 > data.size()) return std::nullopt;
            std::string str = data.substr(pos, len);
            pos += len + 2;
            return RespValue{RespType::BulkString, str};
        }
        case '*': {
            size_t end = data.find('\r', pos);
            if (end == std::string::npos || data[end+1] != '\n') return std::nullopt;
            std::string count_str = data.substr(pos, end - pos);
            long long count = std::stoll(count_str);
            pos = end + 2;
            std::vector<RespValue> array;
            for (long long i = 0; i < count; ++i) {
                auto val = parse_value(data, pos);
                if (!val) return std::nullopt;
                array.push_back(*val);
            }
            return RespValue{RespType::Array, "", 0, array};
        }
        default:
            return std::nullopt;
    }
}
