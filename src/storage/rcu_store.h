#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <chrono>
#include <string>

using Clock = std::chrono::steady_clock;

template <typename K, typename V>
class RcuStore {
public:
    struct Entry {
        V value;
        std::optional<std::chrono::seconds> ttl;
        std::optional<Clock::time_point> expires;

        bool is_expired() const {
            return expires && Clock::now() > *expires;
        }
    };

    // Readers: acquire shared_ptr snapshot atomically — zero lock contention
    // Multiple readers proceed concurrently with no blocking
    std::optional<V> get(const K& key) const {
        auto snapshot = std::atomic_load_explicit(
            &data_, std::memory_order_acquire
        );
        auto it = snapshot->find(key);
        if (it == snapshot->end()) return std::nullopt;
        if (it->second.is_expired()) return std::nullopt;
        return it->second.value;
    }

    // Writers: copy-on-write — create new map, swap atomically
    // While writer is copying, readers still see the old map safely
    void set(const K& key, V value, std::optional<std::chrono::seconds> ttl = {}) {
        std::lock_guard<std::mutex> lock(write_mutex_);

        auto old = std::atomic_load_explicit(&data_, std::memory_order_relaxed);
        auto updated = std::make_shared<std::unordered_map<K, Entry>>(*old);  // copy

        (*updated)[key] = Entry{
            .value   = std::move(value),
            .ttl     = ttl,
            .expires = ttl ? std::optional{Clock::now() + *ttl} : std::nullopt
        };

        // Atomic swap — readers either see old or new, never torn state
        std::atomic_store_explicit(&data_, updated, std::memory_order_release);

        // TODO: lru_.touch(key);
        // TODO: wal_.append(WalEntry::set(key, value, ttl));
    }

    std::optional<V> del(const K& key) {
        std::lock_guard<std::mutex> lock(write_mutex_);

        auto old = std::atomic_load_explicit(&data_, std::memory_order_relaxed);
        auto updated = std::make_shared<std::unordered_map<K, Entry>>(*old);

        auto it = updated->find(key);
        if (it == updated->end()) return std::nullopt;
        V value = std::move(it->second.value);
        updated->erase(it);

        std::atomic_store_explicit(&data_, updated, std::memory_order_release);

        // TODO: wal_.append(WalEntry::del(key));
        return value;
    }

private:
    using HashMap = std::unordered_map<K, Entry>;
    std::shared_ptr<const HashMap> data_ = std::make_shared<HashMap>();
    std::mutex write_mutex_;    // serialises writes — reads are lock-free
    // LruTracker<K> lru_;
    // WriteAheadLog wal_;
};
