# QuantSimX

QuantSimX is an HFT-focused C++ project scaffold for ingesting Binance market data, rebuilding the level-2 orderbook, computing VWAP (using low-granularity trades initially), and visualizing with ImGui.

This repository contains a minimal CMake scaffold and starter headers/source to begin implementation.

Dependencies (recommended)
- `websocketpp` or `boost::beast` + `boost::asio` — WebSocket ingestion (w/ TLS)
- `nlohmann/json` — JSON parsing
- `spdlog` — logging
- `imgui` + `glfw` + `opengl` — visualization UI

Build (quick)
```bash
mkdir -p build && cd build
cmake ..
cmake --build . -- -j
./quantsimx
```

Installing recommended deps on Debian/Ubuntu (example):
```bash
sudo apt update
sudo apt install -y libasio-dev libnlohmann-json3-dev libspdlog-dev libglfw3-dev libglew-dev libssl-dev
# For websocketpp, you can install via package manager or add as a submodule
```

Next steps
- Implement REST snapshot fetch and depth diff application per Binance rules.
- Implement WebSocket client to subscribe to `btcusdt@depth@100ms` and `btcusdt@aggTrade`.
- Parse messages, rebuild orderbook, compute VWAP (using `aggTrade` initially), write CSV rows.
- Add ImGui visualization for top-of-book, VWAP, and trade stream.

**Implemented Features**
- ✅ WebSocket data ingestion (Binance Spot depth + aggTrade)
- ✅ Orderbook reconstruction (bid/ask levels with efficient map-based storage)
- ✅ VWAP, SMA, RSI indicator computation
- ✅ Signal generation (RSI overbought/oversold detection, VWAP crossovers)
- ✅ CSV export with all market data and signals

**Signal Generation Logic**
- **BUY**: Price crosses VWAP upward OR RSI exits oversold zone (<30)
- **SELL**: Price crosses VWAP downward OR RSI exits overbought zone (>70)

QuantSimX is a Qt-based, high-performance quantitative finance platform featuring advanced option pricing models, a parallelized risk engine for VaR and tail risk, real-time 3D Greeks visualization, limit order book simulation, and asset price action modeling. Ideal for quants and risk analysts.
