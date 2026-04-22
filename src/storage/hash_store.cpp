#include "rcu_store.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>

class HashStore {
public:
    long long hset(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fields) {
        auto hash = get_hash(key);
        long long added = 0;
        for (const auto& [field, value] : fields) {
            if (hash.find(field) == hash.end()) added++;
            hash[field] = value;
        }
        set_hash(key, hash);
        return added;
    }

    std::optional<std::string> hget(const std::string& key, const std::string& field) {
        auto hash = get_hash(key);
        auto it = hash.find(field);
        return it != hash.end() ? std::optional{it->second} : std::nullopt;
    }

    std::unordered_map<std::string, std::string> hgetall(const std::string& key) {
        return get_hash(key);
    }

private:
    using Hash = std::unordered_map<std::string, std::string>;
    RcuStore<std::string, Hash> store_;

    Hash get_hash(const std::string& key) {
        auto opt = store_.get(key);
        return opt ? *opt : Hash{};
    }

    void set_hash(const std::string& key, const Hash& hash) {
        store_.set(key, hash);
    }
};
