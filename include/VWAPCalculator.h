#pragma once

#include <vector>
#include <cstdint>
#include <deque>

class VWAPCalculator {
public:
    VWAPCalculator(size_t sma_period = 20, size_t rsi_period = 14);
    
    /// Add a price and volume update
    void add_price(uint64_t timestamp_ms, double price, double volume);
    
    /// Get current VWAP
    double get_vwap() const { return vwap_; }
    
    /// Get Simple Moving Average
    double get_sma() const { return sma_; }
    
    /// Get Relative Strength Index (0-100)
    double get_rsi() const { return rsi_; }
    
    /// Get number of samples processed
    size_t samples_count() const { return samples_.size(); }
    
    /// Recompute all indicators (call after adding prices)
    void update();
    
private:
    struct Sample {
        uint64_t timestamp_ms;
        double price;
        double volume;
    };
    
    void compute_vwap();
    void compute_sma();
    void compute_rsi();
    
    size_t sma_period_;
    size_t rsi_period_;
    std::deque<Sample> samples_;
    
    double vwap_ = 0.0;
    double sma_ = 0.0;
    double rsi_ = 0.0;
};
