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

Next steps
- Implement REST snapshot fetch and depth diff application per Binance rules.
- Implement WebSocket client to subscribe to `btcusdt@depth@100ms` and `btcusdt@aggTrade`.
- Parse messages, rebuild orderbook, compute VWAP (using `aggTrade` initially), write CSV rows.
- Add ImGui visualization for top-of-book, VWAP, and trade stream.

QuantSimX is a Qt-based, high-performance quantitative finance platform featuring advanced option pricing models, a parallelized risk engine for VaR and tail risk, real-time 3D Greeks visualization, limit order book simulation, and asset price action modeling. Ideal for quants and risk analysts.
