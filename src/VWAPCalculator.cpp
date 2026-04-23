#include "VWAPCalculator.h"
#include <cmath>

VWAPCalculator::VWAPCalculator(size_t sma_period, size_t rsi_period)
    : samples_(RingBuffer<Sample, MAX_SAMPLES>())
{
}

void VWAPCalculator::add_price(uint64_t timestamp_ms, double price, double volume) {
    Sample s{timestamp_ms, price, volume};
    samples_.push(s);
    
    cumulative_volume_price_ += price * volume;
    cumulative_volume_ += volume;
    
    if (cumulative_volume_ > 0) {
        vwap_ = cumulative_volume_price_ / cumulative_volume_;
    }
}

void VWAPCalculator::update() {
    compute_sma();
    compute_rsi();
}

void VWAPCalculator::compute_sma() {
    if (samples_.size() < SMA_WINDOW) {
        sma_ = 0.0;
        return;
    }
    
    double sum = 0.0;
    std::size_t start = samples_.size() - SMA_WINDOW;
    for (std::size_t i = 0; i < SMA_WINDOW; ++i) {
        sum += samples_.begin()[start + i].price;
    }
    sma_ = sum / SMA_WINDOW;
}

void VWAPCalculator::compute_rsi() {
    if (samples_.size() < RSI_WINDOW + 1) {
        rsi_ = 50.0;
        return;
    }
    
    gain_sum_ = 0.0;
    loss_sum_ = 0.0;
    
    auto* data = samples_.begin();
    double prev_price = data[0].price;
    
    for (std::size_t i = 1; i < RSI_WINDOW + 1; ++i) {
        double change = data[i].price - prev_price;
        if (change > 0) {
            gain_sum_ += change;
        } else {
            loss_sum_ += std::abs(change);
        }
        prev_price = data[i].price;
    }
    
    double avg_gain = gain_sum_ / RSI_WINDOW;
    double avg_loss = loss_sum_ / RSI_WINDOW;
    
    if (avg_loss == 0) {
        rsi_ = 100.0;
    } else {
        double rs = avg_gain / avg_loss;
        rsi_ = 100.0 - (100.0 / (1.0 + rs));
    }
}