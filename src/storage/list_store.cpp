#include "rcu_store.h"
#include <deque>
#include <string>
#include <vector>
#include <optional>

class ListStore {
public:
    long long lpush(const std::string& key, const std::vector<std::string>& values) {
        auto list = get_list(key);
        for (auto it = values.rbegin(); it != values.rend(); ++it) {
            list.push_front(*it);
        }
        set_list(key, list);
        return list.size();
    }

    long long rpush(const std::string& key, const std::vector<std::string>& values) {
        auto list = get_list(key);
        for (const auto& val : values) {
            list.push_back(val);
        }
        set_list(key, list);
        return list.size();
    }

    std::vector<std::string> lrange(const std::string& key, long long start, long long end) {
        auto list = get_list(key);
        if (list.empty()) return {};
        // Handle negative indices
        if (start < 0) start += list.size();
        if (end < 0) end += list.size();
        start = std::max(0LL, start);
        end = std::min(static_cast<long long>(list.size()) - 1, end);
        if (start > end) return {};
        std::vector<std::string> result;
        auto it = list.begin();
        std::advance(it, start);
        for (long long i = start; i <= end; ++i) {
            result.push_back(*it);
            ++it;
        }
        return result;
    }

private:
    using List = std::deque<std::string>;
    RcuStore<std::string, List> store_;

    List get_list(const std::string& key) {
        auto opt = store_.get(key);
        return opt ? *opt : List{};
    }

    void set_list(const std::string& key, const List& list) {
        store_.set(key, list);
    }
};
