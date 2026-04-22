#include "rcu_store.h"
#include <string>
#include <optional>
#include <chrono>

class StringStore {
public:
    std::optional<std::string> get(const std::string& key) {
        return store_.get(key);
    }

    void set(const std::string& key, const std::string& value, std::optional<std::chrono::seconds> ttl = {}) {
        store_.set(key, value, ttl);
    }

    std::optional<std::string> del(const std::string& key) {
        return store_.del(key);
    }

    std::optional<long long> incr(const std::string& key) {
        auto val = get(key);
        long long num = val ? std::stoll(*val) : 0;
        num++;
        set(key, std::to_string(num));
        return num;
    }

    std::optional<long long> decr(const std::string& key) {
        auto val = get(key);
        long long num = val ? std::stoll(*val) : 0;
        num--;
        set(key, std::to_string(num));
        return num;
    }

    bool expire(const std::string& key, std::chrono::seconds ttl) {
        auto val = get(key);
        if (!val) return false;
        set(key, *val, ttl);
        return true;
    }

    std::optional<long long> ttl(const std::string& key) {
        // TODO: implement TTL query
        return std::nullopt;
    }

    bool persist(const std::string& key) {
        auto val = get(key);
        if (!val) return false;
        set(key, *val, {});
        return true;
    }

private:
    RcuStore<std::string, std::string> store_;
};
