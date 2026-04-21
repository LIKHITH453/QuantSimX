#include "TradingAlgorithms.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <deque>

SignalEvent make_signal(uint64_t ts, Signal sig, const std::string& reason, double price, double aux) {
    SignalEvent evt;
    evt.timestamp_ms = ts;
    evt.signal = sig;
    evt.reason = reason;
    evt.price = price;
    evt.auxiliary = aux;
    return evt;
}

class RSIMeanReversion : public TradingAlgorithm {
public:
    RSIMeanReversion(double oversold = 30.0, double overbought = 70.0)
        : oversold_(oversold), overbought_(overbought) {}
    
    const char* name() const override { return "RSI Mean Reversion"; }
    const char* description() const override { return "Buy when RSI < oversold, sell when RSI > overbought"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; }
    
    void configure(const std::string& params) override {
        std::istringstream ss(params);
        char delim;
        ss >> oversold_ >> delim >> overbought_;
    }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.rsi <= 0) return Signal::NONE;
        
        double rsi = tick.rsi;
        Signal sig = Signal::NONE;
        
        if (rsi < oversold_ && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            emit_signal(sig, "RSI oversold", rsi);
        } else if (rsi > overbought_ && in_position_) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "RSI overbought", rsi);
        }
        return sig;
    }
    
private:
    double oversold_, overbought_;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("RSI Mean Reversion", RSIMeanReversion);

class MACDCrossover : public TradingAlgorithm {
public:
    MACDCrossover(int fast = 12, int slow = 26, int signal = 9)
        : fast_period_(fast), slow_period_(slow), signal_period_(signal) {}
    
    const char* name() const override { return "MACD Crossover"; }
    const char* description() const override { return "Buy when MACD crosses above signal, sell when below"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; ema_fast_ = 0; ema_slow_ = 0; }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.price <= 0) return Signal::NONE;
        
        if (ema_fast_ == 0) {
            ema_fast_ = tick.price;
            ema_slow_ = tick.price;
            macd_line_ = 0;
            signal_line_ = 0;
            return Signal::NONE;
        }
        
        double alpha_fast = 2.0 / (fast_period_ + 1);
        double alpha_slow = 2.0 / (slow_period_ + 1);
        double alpha_signal = 2.0 / (signal_period_ + 1);
        
        ema_fast_ = tick.price * alpha_fast + ema_fast_ * (1 - alpha_fast);
        ema_slow_ = tick.price * alpha_slow + ema_slow_ * (1 - alpha_slow);
        
        double new_macd = ema_fast_ - ema_slow_;
        double new_signal = new_macd * alpha_signal + signal_line_ * (1 - alpha_signal);
        
        bool was_above = macd_line_ > signal_line_;
        bool is_above = new_macd > new_signal;
        
        Signal sig = Signal::NONE;
        if (is_above && !was_above && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            emit_signal(sig, "MACD bullish crossover", new_macd - new_signal);
        } else if (!is_above && was_above && in_position_) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "MACD bearish crossover", new_macd - new_signal);
        }
        
        macd_line_ = new_macd;
        signal_line_ = new_signal;
        return sig;
    }
    
private:
    int fast_period_, slow_period_, signal_period_;
    double ema_fast_ = 0, ema_slow_ = 0;
    double macd_line_ = 0, signal_line_ = 0;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("MACD Crossover", MACDCrossover);

class DualSMA : public TradingAlgorithm {
public:
    DualSMA(int fast = 10, int slow = 30)
        : fast_period_(fast), slow_period_(slow) {}
    
    const char* name() const override { return "Dual SMA Crossover"; }
    const char* description() const override { return "Buy when fast SMA crosses above slow SMA"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; prices_.clear(); }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.price <= 0) return Signal::NONE;
        
        prices_.push_back(tick.price);
        while (prices_.size() > static_cast<size_t>(slow_period_)) {
            prices_.erase(prices_.begin());
        }
        
        if (prices_.size() < static_cast<size_t>(slow_period_)) return Signal::NONE;
        
        double fast_sum = 0, slow_sum = 0;
        for (int i = 0; i < fast_period_; ++i) {
            fast_sum += prices_[prices_.size() - 1 - i];
        }
        for (int i = 0; i < slow_period_; ++i) {
            slow_sum += prices_[prices_.size() - 1 - i];
        }
        double fast_sma = fast_sum / fast_period_;
        double slow_sma = slow_sum / slow_period_;
        
        bool was_above = prev_fast_sma_ > prev_slow_sma_;
        bool is_above = fast_sma > slow_sma;
        
        Signal sig = Signal::NONE;
        if (is_above && !was_above && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            emit_signal(sig, "Fast SMA crossed above slow SMA", fast_sma - slow_sma);
        } else if (!is_above && was_above && in_position_) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "Fast SMA crossed below slow SMA", fast_sma - slow_sma);
        }
        
        prev_fast_sma_ = fast_sma;
        prev_slow_sma_ = slow_sma;
        return sig;
    }
    
private:
    int fast_period_, slow_period_;
    std::deque<double> prices_;
    double prev_fast_sma_ = 0, prev_slow_sma_ = 0;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("Dual SMA Crossover", DualSMA);

class MomentumBurst : public TradingAlgorithm {
public:
    MomentumBurst(double threshold_pct = 0.5, int window_ms = 5000)
        : threshold_pct_(threshold_pct), window_ms_(window_ms) {}
    
    const char* name() const override { return "Momentum Burst"; }
    const char* description() const override { return "Buy when price jumps X% in Y ms, sell on reversal"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; burst_prices_.clear(); }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.price <= 0) return Signal::NONE;
        
        uint64_t cutoff = tick.timestamp_ms - window_ms_;
        while (!burst_prices_.empty() && burst_prices_.front().first < cutoff) {
            burst_prices_.pop_front();
        }
        burst_prices_.push_back({tick.timestamp_ms, tick.price});
        
        if (burst_prices_.size() < 2) return Signal::NONE;
        
        double earliest = burst_prices_.front().second;
        double change_pct = (tick.price - earliest) / earliest * 100.0;
        
        Signal sig = Signal::NONE;
        if (change_pct > threshold_pct_ && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            entry_price_ = tick.price;
            emit_signal(sig, "Momentum burst +" + std::to_string(change_pct) + "%", change_pct);
        } else if (in_position_) {
            double drawdown = (tick.price - entry_price_) / entry_price_ * 100.0;
            if (drawdown < -threshold_pct_ || (change_pct < -threshold_pct_ && change_pct < 0)) {
                sig = Signal::SELL;
                in_position_ = false;
                emit_signal(sig, "Momentum reversal", drawdown);
            }
        }
        return sig;
    }
    
private:
    double threshold_pct_;
    int window_ms_;
    std::deque<std::pair<uint64_t, double>> burst_prices_;
    bool in_position_ = false;
    double entry_price_ = 0;
};
REGISTER_ALGORITHM("Momentum Burst", MomentumBurst);

class Supertrend : public TradingAlgorithm {
public:
    Supertrend(int period = 10, double multiplier = 3.0)
        : period_(period), multiplier_(multiplier) {}
    
    const char* name() const override { return "Supertrend"; }
    const char* description() const override { return "Trend following using ATR with multiplier"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; tr_list_.clear(); }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.price <= 0) return Signal::NONE;
        
        if (!price_history_.empty()) {
            double tr1 = tick.price - price_history_.back();
            double tr2 = std::abs(tick.price - price_history_.back());
            double tr3 = std::abs(tick.price - price_history_.back());
            double tr = std::max(tr1, std::max(tr2, tr3));
            tr_list_.push_back(tr);
            while (tr_list_.size() > static_cast<size_t>(period_)) {
                tr_list_.pop_front();
            }
        }
        price_history_.push_back(tick.price);
        while (price_history_.size() > static_cast<size_t>(period_)) {
            price_history_.pop_front();
        }
        
        if (tr_list_.size() < static_cast<size_t>(period_)) return Signal::NONE;
        
        double atr = 0;
        for (double v : tr_list_) atr += v;
        atr /= period_;
        
        double upper_band = tick.price + multiplier_ * atr;
        double lower_band = tick.price - multiplier_ * atr;
        
        if (upper_band_ > 0) {
            upper_band = std::min(upper_band, upper_band_);
            lower_band = std::max(lower_band, lower_band_);
        }
        
        Signal sig = Signal::NONE;
        if (tick.price > upper_band_ && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            emit_signal(sig, "Supertrend long", atr);
        } else if (tick.price < lower_band_ && in_position_) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "Supertrend short", atr);
        }
        
        upper_band_ = upper_band;
        lower_band_ = lower_band;
        return sig;
    }
    
private:
    int period_;
    double multiplier_;
    std::deque<double> price_history_;
    std::deque<double> tr_list_;
    double upper_band_ = 0, lower_band_ = 0;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("Supertrend", Supertrend);

class VWAPBreakout : public TradingAlgorithm {
public:
    VWAPBreakout(double breakout_threshold_pct = 0.1)
        : threshold_pct_(breakout_threshold_pct) {}
    
    const char* name() const override { return "VWAP Breakout"; }
    const char* description() const override { return "Buy when price breaks VWAP with volume"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; vwap_history_.clear(); }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.vwap <= 0 || tick.price <= 0) return Signal::NONE;
        
        double vwap = tick.vwap;
        vwap_history_.push_back({tick.timestamp_ms, vwap, tick.volume});
        
        double change_pct = (tick.price - vwap) / vwap * 100.0;
        
        Signal sig = Signal::NONE;
        if (change_pct > threshold_pct_ && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            emit_signal(sig, "VWAP breakout +" + std::to_string(change_pct) + "%", change_pct);
        } else if (change_pct < -threshold_pct_ && !in_position_) {
            sig = Signal::SELL;
            in_position_ = true;
            emit_signal(sig, "VWAP breakdown " + std::to_string(change_pct) + "%", change_pct);
        } else if (in_position_ && std::abs(change_pct) < threshold_pct_ * 0.5) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "Price returned to VWAP", change_pct);
        }
        return sig;
    }
    
private:
    double threshold_pct_;
    std::deque<std::tuple<uint64_t, double, double>> vwap_history_;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("VWAP Breakout", VWAPBreakout);

class TradeSizeSpike : public TradingAlgorithm {
public:
    TradeSizeSpike(double spike_multiplier = 3.0, int window = 20)
        : spike_multiplier_(spike_multiplier), window_(window) {}
    
    const char* name() const override { return "Trade Size Spike"; }
    const char* description() const override { return "Buy when large trades appear, sell when they disappear"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; trade_sizes_.clear(); }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.volume <= 0) return Signal::NONE;
        
        trade_sizes_.push_back(tick.volume);
        while (trade_sizes_.size() > static_cast<size_t>(window_)) {
            trade_sizes_.pop_front();
        }
        
        if (trade_sizes_.size() < static_cast<size_t>(window_)) return Signal::NONE;
        
        double avg_size = 0;
        for (double v : trade_sizes_) avg_size += v;
        avg_size /= trade_sizes_.size();
        
        Signal sig = Signal::NONE;
        if (tick.volume > avg_size * spike_multiplier_ && !in_position_) {
            sig = tick.is_buy_tick ? Signal::BUY : Signal::SELL;
            in_position_ = true;
            emit_signal(sig, "Large trade detected", tick.volume / avg_size);
        } else if (in_position_ && tick.volume < avg_size * 1.5) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "Trade size normalized", tick.volume / avg_size);
        }
        return sig;
    }
    
private:
    double spike_multiplier_;
    int window_;
    std::deque<double> trade_sizes_;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("Trade Size Spike", TradeSizeSpike);

class DeltaMomentum : public TradingAlgorithm {
public:
    DeltaMomentum(double threshold = 1000, int window = 10)
        : threshold_(threshold), window_(window) {}
    
    const char* name() const override { return "Delta Momentum"; }
    const char* description() const override { return "Buy when cumulative delta turns positive"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; deltas_.clear(); }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        
        double delta = tick.is_buy_tick ? tick.volume : -tick.volume;
        cum_delta_ += delta;
        
        deltas_.push_back(cum_delta_);
        while (deltas_.size() > static_cast<size_t>(window_)) {
            deltas_.pop_front();
        }
        
        if (deltas_.size() < 2) return Signal::NONE;
        
        bool was_positive = deltas_[deltas_.size() - 2] > threshold_;
        bool is_positive = cum_delta_ > threshold_;
        
        Signal sig = Signal::NONE;
        if (is_positive && !was_positive && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            emit_signal(sig, "Delta turned positive", cum_delta_);
        } else if (!is_positive && was_positive && in_position_) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "Delta turned negative", cum_delta_);
        }
        return sig;
    }
    
private:
    double threshold_;
    int window_;
    std::deque<double> deltas_;
    double cum_delta_ = 0;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("Delta Momentum", DeltaMomentum);

class SpreadCompression : public TradingAlgorithm {
public:
    SpreadCompression(double narrow_bps = 5.0, double wide_bps = 20.0)
        : narrow_bps_(narrow_bps), wide_bps_(wide_bps) {}
    
    const char* name() const override { return "Spread Compression"; }
    const char* description() const override { return "Buy when spread narrows, sell when it widens"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.bid_price <= 0 || tick.ask_price <= 0) return Signal::NONE;
        
        double spread = (tick.ask_price - tick.bid_price) / tick.mid_price() * 10000;
        
        Signal sig = Signal::NONE;
        if (spread < narrow_bps_ && !in_position_) {
            sig = Signal::BUY;
            in_position_ = true;
            emit_signal(sig, "Spread compressed", spread);
        } else if (spread > wide_bps_ && in_position_) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "Spread widened", spread);
        }
        return sig;
    }
    
private:
    double narrow_bps_, wide_bps_;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("Spread Compression", SpreadCompression);

class VolatilityMomentum : public TradingAlgorithm {
public:
    VolatilityMomentum(int lookback = 20, double burst_threshold = 2.0)
        : lookback_(lookback), burst_threshold_(burst_threshold) {}
    
    const char* name() const override { return "Volatility Momentum"; }
    const char* description() const override { return "Trade momentum bursts in low volatility regimes"; }
    
    void reset() override { signals_.clear(); last_signal_ = {}; returns_.clear(); }
    
    Signal update(const TickData& tick) override {
        last_tick_ = tick;
        if (tick.price <= 0 || price_history_.size() < 2) {
            price_history_.push_back(tick.price);
            return Signal::NONE;
        }
        
        double ret = (tick.price - price_history_.back()) / price_history_.back() * 100;
        price_history_.push_back(tick.price);
        while (price_history_.size() > static_cast<size_t>(lookback_)) {
            price_history_.pop_front();
        }
        
        returns_.push_back(std::abs(ret));
        while (returns_.size() > static_cast<size_t>(lookback_)) {
            returns_.pop_front();
        }
        
        if (returns_.size() < static_cast<size_t>(lookback_)) return Signal::NONE;
        
        double avg_vol = 0, var = 0;
        for (double r : returns_) avg_vol += r;
        avg_vol /= returns_.size();
        for (double r : returns_) var += (r - avg_vol) * (r - avg_vol);
        double stddev = std::sqrt(var / returns_.size());
        
        Signal sig = Signal::NONE;
        if (stddev < avg_vol * 0.5 && std::abs(ret) > avg_vol * burst_threshold_ && !in_position_) {
            sig = ret > 0 ? Signal::BUY : Signal::SELL;
            in_position_ = true;
            emit_signal(sig, "Vol momentum burst", stddev / avg_vol);
        } else if (in_position_ && stddev > avg_vol * 2.0) {
            sig = Signal::SELL;
            in_position_ = false;
            emit_signal(sig, "Volatility expansion", stddev / avg_vol);
        }
        return sig;
    }
    
private:
    int lookback_;
    double burst_threshold_;
    std::deque<double> price_history_;
    std::deque<double> returns_;
    bool in_position_ = false;
};
REGISTER_ALGORITHM("Volatility Momentum", VolatilityMomentum);
