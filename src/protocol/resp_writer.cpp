#include "resp_writer.h"
#include <sstream>

std::string RespWriter::write_simple_string(const std::string& str) {
    return "+" + str + "\r\n";
}

std::string RespWriter::write_error(const std::string& str) {
    return "-" + str + "\r\n";
}

std::string RespWriter::write_integer(long long num) {
    return ":" + std::to_string(num) + "\r\n";
}

std::string RespWriter::write_bulk_string(const std::string& str) {
    if (str.empty()) return "$-1\r\n";
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

std::string RespWriter::write_array(const std::vector<std::string>& array) {
    std::string result = "*" + std::to_string(array.size()) + "\r\n";
    for (const auto& item : array) {
        result += write_bulk_string(item);
    }
    return result;
}
