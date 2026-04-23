#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

template<typename T, std::size_t Capacity>
class RingBuffer {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    inline bool push(const T& item) {
        if (count_ >= Capacity) return false;
        data_[write_pos_] = item;
        write_pos_ = (write_pos_ + 1) & (Capacity - 1);
        ++count_;
        return true;
    }

    inline bool pop(T& item) {
        if (count_ == 0) return false;
        item = data_[read_pos_];
        read_pos_ = (read_pos_ + 1) & (Capacity - 1);
        --count_;
        return true;
    }

    inline T& back() {
        return data_[(write_pos_ - 1) & (Capacity - 1)];
    }

    inline const T& back() const {
        return data_[(write_pos_ - 1) & (Capacity - 1)];
    }

    inline T& front() {
        return data_[read_pos_];
    }

    inline const T& front() const {
        return data_[read_pos_];
    }

    inline void clear() {
        write_pos_ = 0;
        read_pos_ = 0;
        count_ = 0;
    }

    inline bool empty() const { return count_ == 0; }
    inline bool full() const { return count_ >= Capacity; }
    inline std::size_t size() const { return count_; }
    inline std::size_t capacity() const { return Capacity; }

    inline T* begin() { return data_.data(); }
    inline const T* begin() const { return data_.data(); }
    inline T* end() { return data_.data() + count_; }
    inline const T* end() const { return data_.data() + count_; }

private:
    alignas(64) std::array<T, Capacity> data_;
    std::size_t write_pos_ = 0;
    std::size_t read_pos_ = 0;
    std::size_t count_ = 0;
};

struct alignas(64) Sample {
    uint64_t timestamp_ms;
    double price;
    double volume;
};

class VWAPCalculator {
public:
    static constexpr std::size_t MAX_SAMPLES = 4096;
    static constexpr std::size_t SMA_WINDOW = 20;
    static constexpr std::size_t RSI_WINDOW = 14;

    VWAPCalculator(size_t sma_period = SMA_WINDOW, size_t rsi_period = RSI_WINDOW);
    
    void add_price(uint64_t timestamp_ms, double price, double volume);
    
    double get_vwap() const { return vwap_; }
    double get_sma() const { return sma_; }
    double get_rsi() const { return rsi_; }
    size_t samples_count() const { return samples_.size(); }
    
    void update();

private:
    void compute_vwap();
    void compute_sma();
    void compute_rsi();
    
    RingBuffer<Sample, MAX_SAMPLES> samples_;
    
    double cumulative_volume_price_ = 0.0;
    double cumulative_volume_ = 0.0;
    double vwap_ = 0.0;
    double sma_ = 0.0;
    double rsi_ = 50.0;
    
    double prev_close_ = 0.0;
    double gain_sum_ = 0.0;
    double loss_sum_ = 0.0;
};