#pragma once

#include <string>
#include <vector>

class RespWriter {
public:
    static std::string write_simple_string(const std::string& str);
    static std::string write_error(const std::string& str);
    static std::string write_integer(long long num);
    static std::string write_bulk_string(const std::string& str);
    static std::string write_array(const std::vector<std::string>& array);
};
