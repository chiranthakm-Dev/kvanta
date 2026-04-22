#include "write_ahead_log.h"
#include <cstring>
#include <unistd.h>

WalEntry WalEntry::set(const std::string& key, const std::string& value, std::optional<std::chrono::seconds> ttl) {
    return {
        .type = Type::Set,
        .key = key,
        .value = value,
        .ttl_ms = ttl ? std::optional{std::chrono::duration_cast<std::chrono::milliseconds>(*ttl)} : std::nullopt
    };
}

WalEntry WalEntry::del(const std::string& key) {
    return {
        .type = Type::Delete,
        .key = key,
        .value = "",
        .ttl_ms = std::nullopt
    };
}

std::vector<char> WalEntry::serialise() const {
    std::vector<char> buf;
    buf.push_back(static_cast<char>(type));
    uint32_t key_len = key.size();
    buf.insert(buf.end(), reinterpret_cast<char*>(&key_len), reinterpret_cast<char*>(&key_len) + sizeof(key_len));
    buf.insert(buf.end(), key.begin(), key.end());
    uint32_t value_len = value.size();
    buf.insert(buf.end(), reinterpret_cast<char*>(&value_len), reinterpret_cast<char*>(&value_len) + sizeof(value_len));
    buf.insert(buf.end(), value.begin(), value.end());
    uint64_t ttl = ttl_ms ? ttl_ms->count() : 0;
    buf.insert(buf.end(), reinterpret_cast<char*>(&ttl), reinterpret_cast<char*>(&ttl) + sizeof(ttl));
    return buf;
}

WalEntry WalEntry::deserialise(const std::vector<char>& data) {
    size_t pos = 0;
    Type type = static_cast<Type>(data[pos++]);
    uint32_t key_len;
    std::memcpy(&key_len, &data[pos], sizeof(key_len));
    pos += sizeof(key_len);
    std::string key(data.begin() + pos, data.begin() + pos + key_len);
    pos += key_len;
    uint32_t value_len;
    std::memcpy(&value_len, &data[pos], sizeof(value_len));
    pos += sizeof(value_len);
    std::string value(data.begin() + pos, data.begin() + pos + value_len);
    pos += value_len;
    uint64_t ttl;
    std::memcpy(&ttl, &data[pos], sizeof(ttl));
    std::optional<std::chrono::milliseconds> ttl_ms = ttl ? std::optional{std::chrono::milliseconds(ttl)} : std::nullopt;
    return {type, key, value, ttl_ms};
}

class WriteAheadLog {
public:
    WriteAheadLog(const std::filesystem::path& dir) : dir_(dir) {
        std::filesystem::create_directories(dir_);
        log_file_.open(dir_ / "wal.log", std::ios::app | std::ios::binary);
        if (!log_file_) {
            throw std::runtime_error("Cannot open WAL file: " + dir_.string());
        }
    }

    void append(const WalEntry& entry) {
        auto bytes = entry.serialise();
        uint32_t len = static_cast<uint32_t>(bytes.size());
        log_file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
        log_file_.write(bytes.data(), bytes.size());
        log_file_.flush();
        ::fsync(log_file_.native_handle());
    }

private:
    std::filesystem::path dir_;
    std::ofstream log_file_;
};
