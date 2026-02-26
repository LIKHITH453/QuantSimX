#include "VWAPCalculator.h"
#include <cmath>
#include <numeric>
#include <iostream>
#include <algorithm>

VWAPCalculator::VWAPCalculator(size_t sma_period, size_t rsi_period)
    : sma_period_(sma_period), rsi_period_(rsi_period) {}

void VWAPCalculator::add_price(uint64_t timestamp_ms, double price, double volume) {
    samples_.push_back({timestamp_ms, price, volume});
}

void VWAPCalculator::update() {
    if (samples_.empty()) return;
    
    compute_vwap();
    compute_sma();
    compute_rsi();
}

void VWAPCalculator::compute_vwap() {
    if (samples_.empty()) {
        vwap_ = 0.0;
        return;
    }
    
    double cumul_pv = 0.0;
    double cumul_vol = 0.0;
    
    for (const auto& sample : samples_) {
        cumul_pv += sample.price * sample.volume;
        cumul_vol += sample.volume;
    }
    
    vwap_ = cumul_vol > 0.0 ? cumul_pv / cumul_vol : 0.0;
}

void VWAPCalculator::compute_sma() {
    if (samples_.empty()) {
        sma_ = 0.0;
        return;
    }
    
    size_t n = std::min(sma_period_, samples_.size());
    double sum = 0.0;
    
    for (size_t i = samples_.size() - n; i < samples_.size(); ++i) {
        sum += samples_[i].price;
    }
    
    sma_ = sum / static_cast<double>(n);
}

void VWAPCalculator::compute_rsi() {
    if (samples_.size() < rsi_period_ + 1) {
        rsi_ = 50.0;  // Neutral RSI when insufficient data
        return;
    }
    
    std::vector<double> gains, losses;
    
    for (size_t i = 1; i < samples_.size(); ++i) {
        double change = samples_[i].price - samples_[i - 1].price;
        if (change > 0) {
            gains.push_back(change);
            losses.push_back(0.0);
        } else {
            gains.push_back(0.0);
            losses.push_back(-change);
        }
    }
    
    // Calculate average gain and loss over RSI period
    size_t period_start = gains.size() > rsi_period_ ? gains.size() - rsi_period_ : 0;
    double avg_gain = 0.0, avg_loss = 0.0;
    
    for (size_t i = period_start; i < gains.size(); ++i) {
        avg_gain += gains[i];
        avg_loss += losses[i];
    }
    
    size_t period_count = gains.size() - period_start;
    avg_gain /= static_cast<double>(period_count);
    avg_loss /= static_cast<double>(period_count);
    
    // Calculate RS and RSI
    double rs = avg_loss > 0.0 ? avg_gain / avg_loss : (avg_gain > 0.0 ? 100.0 : 0.0);
    rsi_ = 100.0 - (100.0 / (1.0 + rs));
}
