#pragma once

#include <memory>
#include <deque>
#include <vector>
#include <ctime>
#include <chrono>
#include <string>

#include "imgui.h"
#include "OrderBook.h"
#include "VWAPCalculator.h"
#include "SignalGenerator.h"
#include "Backtester.h"

struct ImDrawData;

struct TradePosition {
    uint64_t entry_time = 0;
    double entry_price = 0.0;
    double exit_price = 0.0;
    double pnl = 0.0;
    double pnl_percent = 0.0;
    bool is_long = true;
};

struct AccountMetrics {
    double equity = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    double daily_pnl = 0.0;
    double max_drawdown = 0.0;
    double run_up = 0.0;
};

enum class ConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING
};

class DashboardUI {
public:
    static DashboardUI& get() {
        static DashboardUI instance;
        return instance;
    }

    bool init(int width, int height);
    void shutdown();
    bool render(const OrderBook& ob, const VWAPCalculator& vwap_calc,
                const SignalGenerator& sig_gen);

    bool should_connect() const { return connect_requested_; }
    bool should_disconnect() const { return disconnect_requested_; }
    bool should_stop() const { return stop_requested_; }
    bool should_backtest() const { return backtest_requested_; }
    bool should_close() const { return close_requested_; }
    void reset_flags();

    void set_connection_status(ConnectionStatus status) { connection_status_ = status; }
    void set_backtest_results(const BacktestMetrics& metrics) {
        backtest_results_ = metrics;
        has_backtest_results_ = true;
    }

    AccountMetrics get_account() const { return account_; }
    void set_account(const AccountMetrics& acc) { account_ = acc; }

private:
    DashboardUI() = default;
    ~DashboardUI() = default;

    void* window_ = nullptr;
    ConnectionStatus connection_status_ = ConnectionStatus::DISCONNECTED;
    
    bool connect_requested_ = false;
    bool disconnect_requested_ = false;
    bool stop_requested_ = false;
    bool backtest_requested_ = false;
    bool close_requested_ = false;
    bool settings_open_ = false;

    std::deque<double> price_history_;
    std::deque<double> vwap_history_;
    std::deque<double> rsi_history_;
    std::deque<TradePosition> closed_trades_;
    
    double displayed_price_ = 0.0;
    double displayed_vwap_ = 0.0;
    float smooth_alpha_ = 0.15f;

    AccountMetrics account_;
    bool has_backtest_results_ = false;
    BacktestMetrics backtest_results_;
    float anim_time_ = 0.0f;

    int selected_tab_ = 0;
    
    void render_header();
    void render_toolbar();
    void render_tabs();
    void render_market_watch_panel(const OrderBook& ob, const VWAPCalculator& vwap);
    void render_orderbook_panel(const OrderBook& ob);
    void render_chart_panel();
    void render_trades_panel();
    void render_performance_panel();
    void render_signals_panel(const SignalGenerator& sig_gen);
    void render_backtest_panel();
    void render_settings_panel();

    void draw_price_chart();
    void draw_equity_curve();
    void draw_pnl_histogram();

    static constexpr int PRICE_HISTORY_MAX = 500;
    static constexpr int TRADES_DISPLAY_MAX = 50;
};

namespace ImGui {
    void StyleColorsTerminal();
    void SeparatorV();
    void TextHighlight(const char* text, ImVec4 color);
    void PanelHeader(const char* title);
}
