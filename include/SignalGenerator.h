#pragma once

#include "Signal.h"
#include <string>
#include <vector>

class SignalGenerator {
public:
    SignalGenerator(double rsi_oversold = 30.0, double rsi_overbought = 70.0);
    
    Signal update(uint64_t timestamp_ms, double price, double vwap, double rsi);
    
    const SignalEvent& last_signal() const { return last_signal_; }
    const std::vector<SignalEvent>& get_signals() const { return signals_; }
    
    std::string get_state_string() const;
    
private:
    double rsi_oversold_;
    double rsi_overbought_;
    
    double prev_price_ = 0.0;
    double prev_vwap_ = 0.0;
    double prev_rsi_ = 50.0;
    bool price_above_vwap_ = false;
    bool rsi_in_oversold_ = false;
    bool rsi_in_overbought_ = false;
    
    SignalEvent last_signal_;
    std::vector<SignalEvent> signals_;
};