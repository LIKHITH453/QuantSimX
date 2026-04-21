#include "TradingEngine.h"
#include <iostream>

TradingEngine::TradingEngine(const Config& config)
    : config_(config),
      vwap_calc_(config.vwap_sma_period, config.rsi_period),
      signal_gen_(config.rsi_oversold, config.rsi_overbought),
      backtester_(config.rsi_oversold, config.rsi_overbought),
      start_time_(Clock::now()) {
}

TradingEngine::~TradingEngine() {
    if (ws_client_ && ws_client_->is_connected()) {
        ws_client_->disconnect();
    }
}

bool TradingEngine::initialize() {
    auto available = AlgorithmRegistry::instance().available();
    if (!available.empty()) {
        std::cout << "[TradingEngine] Available algorithms:\n";
        for (const auto& name : available) {
            std::cout << "  - " << name << "\n";
        }
    }
    
    ws_client_ = BinanceWebSocketClient::create(orderbook_);
    
    if (!load_market_data()) {
        std::cerr << "[TradingEngine] Warning: Failed to load market data for backtester\n";
    }
    
    return true;
}

void TradingEngine::set_on_trade_callback(std::function<void(double, double, uint64_t)> callback) {
    on_trade_callback_ = callback;
}

void TradingEngine::set_on_signal_callback(std::function<void(Signal, const std::string&)> callback) {
    on_signal_callback_ = callback;
}

void TradingEngine::start_data_collection() {
    if (state_.load() == State::CONNECTED) {
        collecting_data_.store(true);
        start_time_ = Clock::now();
    }
}

void TradingEngine::stop_data_collection() {
    collecting_data_.store(false);
    state_.store(State::STOPPED);
}

void TradingEngine::disconnect() {
    collecting_data_.store(false);
    if (ws_client_ && ws_client_->is_connected()) {
        ws_client_->disconnect();
    }
    state_.store(State::IDLE);
}

void TradingEngine::reconnect(const std::string& symbol) {
    if (state_.load() == State::IDLE || state_.load() == State::STOPPED) {
        state_.store(State::CONNECTING);
        ws_client_->connect(symbol);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (ws_client_->is_connected()) {
            state_.store(State::CONNECTED);
        } else {
            state_.store(State::IDLE);
        }
    }
}

bool TradingEngine::is_connected() const {
    return ws_client_ && ws_client_->is_connected();
}

TradingEngine::State TradingEngine::get_state() const {
    return state_.load();
}

void TradingEngine::process_trade(double price, double qty, uint64_t timestamp) {
    if (!collecting_data_.load()) return;
    
    tick_buffer_.push(timestamp, price, qty, 
                      orderbook_.get_best_bid(), 
                      orderbook_.get_best_ask());
}

void TradingEngine::process_pending_ticks() {
    uint64_t ts;
    double price, qty, bid, ask;
    while (tick_buffer_.pop(ts, price, qty, bid, ask)) {
        process_tick(price, qty, ts);
        if (on_trade_callback_) {
            on_trade_callback_(price, qty, ts);
        }
    }
}

void TradingEngine::process_tick(double price, double qty, uint64_t timestamp) {
    vwap_calc_.add_price(timestamp, price, qty);
    vwap_calc_.update();
    
    Signal sig = signal_gen_.update(timestamp, price, vwap_calc_.get_vwap(), vwap_calc_.get_rsi());
    sample_count_.fetch_add(1);
    
    if (sig != Signal::NONE && on_signal_callback_) {
        on_signal_callback_(sig, signal_gen_.last_signal().reason);
    }
}

void TradingEngine::run_backtest() {
    if (backtester_.get_bars().empty()) {
        std::cerr << "[TradingEngine] No market data loaded for backtest\n";
        return;
    }
    
    state_.store(State::RUNNING_BACKTEST);
    auto metrics = backtester_.run();
    state_.store(State::CONNECTED);
}

bool TradingEngine::load_market_data() {
    return backtester_.load_csv("market_data.csv");
}
