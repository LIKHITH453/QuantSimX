#ifndef TICK_BUFFER_H
#define TICK_BUFFER_H

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

class TickBuffer {
public:
    static constexpr std::size_t SIZE = 1024;
    
    struct Slot {
        std::atomic<bool> ready{false};
        uint64_t timestamp_ms = 0;
        double price = 0.0;
        double qty = 0.0;
        double bid = 0.0;
        double ask = 0.0;
    };
    
    inline bool push(uint64_t ts, double price, double qty, double bid, double ask) {
        std::size_t wp = write_pos_.load(std::memory_order_relaxed);
        std::size_t next_wp = (wp + 1) % SIZE;
        
        if (next_wp == read_pos_.load(std::memory_order_acquire)) {
            return false;
        }
        
        Slot& slot = slots_[wp];
        slot.timestamp_ms = ts;
        slot.price = price;
        slot.qty = qty;
        slot.bid = bid;
        slot.ask = ask;
        slot.ready.store(true, std::memory_order_release);
        
        write_pos_.store(next_wp, std::memory_order_release);
        return true;
    }
    
    inline bool pop(uint64_t& ts, double& price, double& qty, double& bid, double& ask) {
        std::size_t rp = read_pos_.load(std::memory_order_relaxed);
        
        if (!slots_[rp].ready.load(std::memory_order_acquire)) {
            return false;
        }
        
        Slot& slot = slots_[rp];
        ts = slot.timestamp_ms;
        price = slot.price;
        qty = slot.qty;
        bid = slot.bid;
        ask = slot.ask;
        slot.ready.store(false, std::memory_order_release);
        
        read_pos_.store((rp + 1) % SIZE, std::memory_order_release);
        return true;
    }
    
    inline std::size_t available() const {
        std::size_t wp = write_pos_.load(std::memory_order_acquire);
        std::size_t rp = read_pos_.load(std::memory_order_acquire);
        return (wp >= rp) ? (wp - rp) : (SIZE - rp + wp);
    }
    
    inline void clear() {
        write_pos_.store(0, std::memory_order_release);
        read_pos_.store(0, std::memory_order_release);
        for (auto& slot : slots_) {
            slot.ready.store(false, std::memory_order_release);
        }
    }
    
private:
    alignas(64) std::array<Slot, SIZE> slots_;
    alignas(64) std::atomic<std::size_t> write_pos_{0};
    alignas(64) std::atomic<std::size_t> read_pos_{0};
};

#endif
