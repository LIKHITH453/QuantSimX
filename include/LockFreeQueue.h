#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

template<typename T, std::size_t Capacity>
class LockFreeQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible");

    LockFreeQueue() : head_(0), tail_(0) {
        for (std::size_t i = 0; i < Capacity; ++i) {
            slots_[i].version.store(i, std::memory_order_relaxed);
        }
    }

    bool push(const T& item) {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[tail & (Capacity - 1)];
        std::size_t version = slot.version.load(std::memory_order_acquire);
        
        if ((tail - head_.load(std::memory_order_acquire)) >= Capacity) {
            return false;
        }
        
        slot.data = item;
        slot.version.store(tail + 1, std::memory_order_release);
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        std::size_t head = head_.load(std::memory_order_relaxed);
        Slot& slot = slots_[head & (Capacity - 1)];
        std::size_t version = slot.version.load(std::memory_order_acquire);
        
        if (version <= head) {
            return false;
        }
        
        item = slot.data;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool is_empty() const {
        return tail_.load(std::memory_order_acquire) <= head_.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        std::size_t tail = tail_.load(std::memory_order_acquire);
        std::size_t head = head_.load(std::memory_order_acquire);
        return tail > head ? tail - head : 0;
    }

    void clear() {
        T dummy;
        while (pop(dummy)) {}
    }

private:
    struct Slot {
        T data;
        std::atomic<std::size_t> version;
    };
    
    alignas(64) std::array<Slot, Capacity> slots_;
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
};

struct alignas(64) QueuedTick {
    uint64_t timestamp_ms = 0;
    double price = 0.0;
    double qty = 0.0;
    double bid = 0.0;
    double ask = 0.0;
};

using TickQueue = LockFreeQueue<QueuedTick, 4096>;