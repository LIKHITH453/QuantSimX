#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

template<typename T>
class SymbolIndex {
public:
    void add(const std::string& symbol, T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_[symbol] = std::move(value);
    }

    T* get(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(symbol);
        return it != index_.end() ? &it->second : nullptr;
    }

    const T* get(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(symbol);
        return it != index_.end() ? &it->second : nullptr;
    }

    bool contains(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.find(symbol) != index_.end();
    }

    void remove(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.erase(symbol);
    }

    std::vector<std::string> symbols() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        result.reserve(index_.size());
        for (const auto& [k, _] : index_) result.push_back(k);
        return result;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, T> index_;
};