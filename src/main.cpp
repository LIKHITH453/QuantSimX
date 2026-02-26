#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include "OrderBook.h"
#include "BinanceRESTClient.h"
#include "BinanceWebSocketClient.h"
#include "CSVWriter.h"
#include "VWAPCalculator.h"
#include "SignalGenerator.h"
#include "DashboardUI.h"
#include "Backtester.h"

int main(int argc, char** argv) {
    std::cout << "=== QuantSimX HFT Platform ===\n";
    std::cout << "Initializing...\n\n";
    
    try {
        // Initialize OrderBook and indicators
        OrderBook ob;
        VWAPCalculator vwap_calc(20, 14);  // SMA period=20, RSI period=14
        SignalGenerator signal_gen(30.0, 70.0);  // RSI oversold/overbought thresholds
        
        // Initialize dashboard UI
        DashboardUI& dashboard = DashboardUI::get();
        if (!dashboard.init(1600, 900)) {
            std::cerr << "[Main] Failed to initialize dashboard\n";
            return 1;
        }
        
        // Fetch REST snapshot
        std::cout << "[Main] Fetching orderbook snapshot from Binance REST API...\n";
        json snapshot = BinanceRESTClient::fetch_depth_snapshot("BTCUSDT", 20);
        
        if (!snapshot.empty()) {
            ob.init_from_snapshot(snapshot);
        } else {
            std::cout << "[Main] Warning: Failed to fetch snapshot, continuing with empty orderbook\n";
        }
        
        // Initialize CSV writer
        CSVWriter csv("market_data.csv");
        csv.write_row("timestamp_ms,event,best_bid,best_ask,mid_price,last_trade_price,last_trade_qty,vwap,sma,rsi,signal,signal_reason");
        
        // Connect WebSocket client (real connection to Binance)
        std::cout << "[Main] Connecting to Binance WebSocket streams...\n";
        BinanceWebSocketClient ws_client(ob);
        ws_client.connect("btcusdt");
        dashboard.set_app_state(AppState::CONNECTING);
        
        // Wait for connection to establish
        std::this_thread::sleep_for(std::chrono::seconds(2));
        dashboard.set_app_state(AppState::CONNECTED);
        
        std::cout << "[Main] Dashboard ready. Running real-time visualization...\n";
        
        // Initialize Backtester for historical analysis
        Backtester backtester(30.0, 70.0);
        bool backtest_loaded = backtester.load_csv("market_data.csv");
        if (backtest_loaded) {
            std::cout << "[Main] Backtester loaded with historical data, ready to analyze\n";
        }
        
        uint64_t sample_count = 0;
        auto frame_count = 0;
        auto start_time = std::chrono::system_clock::now();
        bool collecting_data = true;
        bool backtest_running = false;
        
        // Main rendering loop - keep running until window closes
        while (true) {
            auto elapsed = std::chrono::system_clock::now() - start_time;
            
            // Check for button presses
            if (dashboard.should_stop_collection()) {
                std::cout << "[Main] User pressed STOP - halting data collection\n";
                collecting_data = false;
                dashboard.set_app_state(AppState::STOPPED);
                ws_client.disconnect();
            }
            
            if (dashboard.should_disconnect()) {
                std::cout << "[Main] User pressed DISCONNECT\n";
                collecting_data = false;
                dashboard.set_app_state(AppState::IDLE);
                ws_client.disconnect();
            }
            
            if (dashboard.should_connect() && dashboard.get_app_state() == AppState::IDLE) {
                std::cout << "[Main] User pressed CONNECT - reconnecting...\n";
                dashboard.set_app_state(AppState::CONNECTING);
                ws_client.connect("btcusdt");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (ws_client.is_connected()) {
                    collecting_data = true;
                    dashboard.set_app_state(AppState::CONNECTED);
                } else {
                    dashboard.set_app_state(AppState::IDLE);
                }
            }
            
            // Handle user-requested backtest (button)
            if (dashboard.should_run_backtest()) {
                std::cout << "[Main] User requested BACKTEST\n";
                if (backtest_loaded) {
                    auto metrics = backtester.run();
                    dashboard.set_backtest_metrics(metrics);
                    std::cout << "[Main] Backtest completed: " << metrics.total_trades << " trades\n";
                    backtest_running = true;
                } else {
                    std::cout << "[Main] Backtest data not loaded (market_data.csv)\n";
                }
                dashboard.reset_button_flags();
            } else {
                dashboard.reset_button_flags();
            }
            
            // Check conditions for exit
            if (dashboard.should_close()) {
                std::cout << "[Main] Dashboard closed by user\n";
                break;
            }
            
            // Timeout after 180 seconds to prevent infinite runs
            if (elapsed > std::chrono::seconds(180)) {
                std::cout << "[Main] 180 second timeout reached\n";
                break;
            }
            // Collect price and volume data for indicators
            double last_price = ob.get_last_trade_price();
            double last_qty = ob.get_last_trade_qty();
            
            if (last_price > 0 && collecting_data) {
                vwap_calc.add_price(
                    std::chrono::system_clock::now().time_since_epoch().count() / 1000000,
                    last_price,
                    last_qty
                );
                vwap_calc.update();
                
                // Generate trading signals
                Signal sig = signal_gen.update(
                    std::chrono::system_clock::now().time_since_epoch().count() / 1000000,
                    last_price,
                    vwap_calc.get_vwap(),
                    vwap_calc.get_rsi()
                );
                sample_count++;
                
                // Write to CSV periodically
                if (sample_count % 5 == 0) {
                    std::ostringstream row;
                    auto now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
                    row << now << ",trade,"
                        << std::fixed << std::setprecision(2)
                        << ob.get_best_bid() << ","
                        << ob.get_best_ask() << ","
                        << ob.get_mid_price() << ","
                        << last_price << ","
                        << std::setprecision(6)
                        << last_qty << ","
                        << std::setprecision(2)
                        << vwap_calc.get_vwap() << ","
                        << vwap_calc.get_sma() << ","
                        << std::setprecision(1)
                        << vwap_calc.get_rsi() << ","
                        << (sig == Signal::BUY ? "BUY" : sig == Signal::SELL ? "SELL" : "NONE") << ","
                        << signal_gen.last_signal().reason;
                    csv.write_row(row.str());
                }
            }
            
            // Run backtester if not already running and data exists
            if (!backtest_running && backtest_loaded && sample_count > 0 && sample_count % 100 == 0) {
                // Run backtest on accumulated data
                backtester.load_csv("market_data.csv");
                auto metrics = backtester.run();
                dashboard.set_backtest_metrics(metrics);
                backtest_running = true;  // Only run once per session after first data
            }
            
            // Render dashboard frame
            if (!dashboard.render_frame(ob, vwap_calc, signal_gen)) {
                break;
            }
            
            frame_count++;
            
            // Small sleep to prevent busy-looping
            std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
        }
        
        // Cleanup
        std::cout << "[Main] Closing dashboard and disconnecting...\n";
        dashboard.shutdown();
        ws_client.disconnect();
        
        std::cout << "[Main] Final Statistics:\n";
        std::cout << "  Frames rendered: " << frame_count << "\n";
        std::cout << "  Samples collected: " << vwap_calc.samples_count() << "\n";
        std::cout << "  Signals generated: " << signal_gen.get_signals().size() << "\n";
        std::cout << "  VWAP: " << std::fixed << std::setprecision(2) << vwap_calc.get_vwap() << "\n";
        std::cout << "  RSI: " << std::setprecision(1) << vwap_calc.get_rsi() << "\n";
        
        csv.flush();
        std::cout << "[Main] Data saved to market_data.csv\n";
        std::cout << "=== Run Complete ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
