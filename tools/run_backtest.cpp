#include <iostream>
#include "Backtester.h"

int main(int argc, char** argv) {
    std::string path = "build/market_data.csv";
    if (argc > 1) path = argv[1];
    Backtester bt(30.0, 70.0);
    bool ok = bt.load_csv(path);
    if (!ok) {
        std::cout << "Failed to load CSV: " << path << "\n";
        return 1;
    }
    auto metrics = bt.run();
    std::cout << "Backtest finished. Trades: " << metrics.total_trades << " Win rate: " << metrics.win_rate << " Total PnL: " << metrics.total_pnl << "\n";
    return 0;
}
