#ifndef TRADING_ENGINE_H
#define TRADING_ENGINE_H

#include <memory>
#include <atomic>
#include <functional>
#include <chrono>
#include <thread>
#include "Config.h"
#include "OrderBook.h"
#include "VWAPCalculator.h"
#include "SignalGenerator.h"
#include "BinanceWebSocketClient.h"
#include "Backtester.h"
#include "TradingAlgorithms.h"
#include "LockFreeQueue.h"

class TradingEngine {
public:
    using Clock = std::chrono::system_clock;
    using Duration = std::chrono::seconds;
    
    enum class State { IDLE, CONNECTING, CONNECTED, STOPPED, RUNNING_BACKTEST };
    
    TradingEngine(const Config& config);
    ~TradingEngine();
    
    bool initialize();
    
    void set_on_trade_callback(std::function<void(double price, double qty, uint64_t timestamp)> callback);
    void set_on_signal_callback(std::function<void(Signal sig, const std::string& reason)> callback);
    
    void start_data_collection();
    void stop_data_collection();
    void disconnect();
    void reconnect(const std::string& symbol);
    
    bool is_connected() const;
    State get_state() const;
    
    void process_trade(double price, double qty, uint64_t timestamp);
    void process_pending_ticks();
    void run_backtest();
    
    OrderBook& get_orderbook() { return orderbook_; }
    VWAPCalculator& get_vwap() { return vwap_calc_; }
    SignalGenerator& get_signal_gen() { return signal_gen_; }
    Backtester& get_backtester() { return backtester_; }
    
    uint64_t get_sample_count() const { return sample_count_; }
    bool should_stop() const { return should_stop_.load(); }
    
private:
    void process_tick(double price, double qty, uint64_t timestamp);
    bool load_market_data();
    
    const Config& config_;
    OrderBook orderbook_;
    VWAPCalculator vwap_calc_;
    SignalGenerator signal_gen_;
    Backtester backtester_;
    std::shared_ptr<BinanceWebSocketClient> ws_client_;
    TickQueue tick_queue_;
    
    std::atomic<State> state_{State::IDLE};
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> collecting_data_{false};
    std::atomic<uint64_t> sample_count_{0};
    
    Clock::time_point start_time_;
    
    std::function<void(double price, double qty, uint64_t timestamp)> on_trade_callback_;
    std::function<void(Signal sig, const std::string& reason)> on_signal_callback_;
};

#endif
