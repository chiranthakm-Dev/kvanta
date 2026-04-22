#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <optional>

struct WalEntry {
    enum class Type { Set, Delete };

    Type type;
    std::string key;
    std::string value;
    std::optional<std::chrono::milliseconds> ttl_ms;

    static WalEntry set(const std::string& key, const std::string& value, std::optional<std::chrono::seconds> ttl);
    static WalEntry del(const std::string& key);

    std::vector<char> serialise() const;
    static WalEntry deserialise(const std::vector<char>& data);
};
