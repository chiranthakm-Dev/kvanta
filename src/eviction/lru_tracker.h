#pragma once

#include <unordered_map>
#include <mutex>
#include <optional>
#include <memory>

template <typename K>
class LruTracker {
    struct Node {
        K key;
        Node* prev = nullptr;
        Node* next = nullptr;
    };

    std::unordered_map<K, Node*> map_;
    Node* head_ = nullptr;   // most recently used
    Node* tail_ = nullptr;   // least recently used
    mutable std::mutex mu_;

public:
    ~LruTracker() {
        std::lock_guard lock(mu_);
        while (head_) {
            Node* temp = head_;
            head_ = head_->next;
            delete temp;
        }
    }

    void touch(const K& key) {
        std::lock_guard lock(mu_);
        if (auto it = map_.find(key); it != map_.end()) {
            move_to_front(it->second);
        } else {
            auto* node = new Node{key, nullptr, head_};
            if (head_) head_->prev = node;
            head_ = node;
            if (!tail_) tail_ = node;
            map_[key] = node;
        }
    }

    std::optional<K> evict_lru() {
        std::lock_guard lock(mu_);
        if (!tail_) return std::nullopt;
        K key = tail_->key;
        remove_node(tail_);
        map_.erase(key);
        return key;
    }

private:
    void move_to_front(Node* node) {
        if (node == head_) return;
        if (node == tail_) tail_ = node->prev;
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        node->next = head_;
        node->prev = nullptr;
        if (head_) head_->prev = node;
        head_ = node;
    }

    void remove_node(Node* node) {
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        if (node == head_) head_ = node->next;
        if (node == tail_) tail_ = node->prev;
        delete node;
    }
};
