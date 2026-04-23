# QuantSimX Architecture

## Overview

QuantSimX is a high-frequency trading (HFT) simulation platform that provides real-time market data visualization, technical indicator computation, and signal generation for algorithmic trading.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                          QuantSimX                                   │
├─────────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │   Binance    │  │  REST API    │  │   OrderBook  │              │
│  │  WebSocket   │──▶│  Snapshot    │──▶│   (Array)    │              │
│  └──────────────┘  └──────────────┘  └──────────────┘              │
│         │                                      │                      │
│         ▼                                      ▼                      │
│  ┌──────────────┐                      ┌──────────────┐            │
│  │  Lock-Free   │                      │   VWAP       │            │
│  │  MPMC Queue  │                      │  Calculator  │            │
│  └──────────────┘                      │  (RingBuf)   │            │
│         │                              └──────────────┘            │
│         ▼                                      │                      │
│  ┌──────────────┐                      ┌──────────────┐            │
│  │   Signal     │◀─────────────────────│    RSI       │            │
│  │  Generator   │                      │  (RingBuf)   │            │
│  └──────────────┘                      └──────────────┘            │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐            │
│  │  Backtester  │  │    CSV       │  │  Dashboard   │            │
│  │              │  │   Writer     │  │    (ImGui)   │            │
│  └──────────────┘  └──────────────┘  └──────────────┘            │
└─────────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Data Ingestion

| Component | Type | Description |
|-----------|------|-------------|
| `BinanceWebSocketClient` | WebSocket | Connects to Binance streams (depth@100ms, aggTrade) |
| `BinanceRESTClient` | HTTP | Fetches orderbook snapshots |
| `LockFreeQueue<TickData, 4096>` | MPMC Queue | Lock-free tick distribution |

### 2. Market Data Processing

| Component | Data Structure | Complexity |
|-----------|---------------|------------|
| `OrderBook` | Flat array [256 levels] | O(n) insert, O(1) best bid/ask |
| `VWAPCalculator` | RingBuffer [4096] + cumulative sum | O(1) VWAP update |
| `SignalGenerator` | State machine | O(1) signal generation |

### 3. Signal Generation

```cpp
Signal = f(price, vwap, rsi)
// BUY: price > vwap && rsi < 30
// SELL: price < vwap && rsi > 70
```

### 4. Backtesting

| Component | Description |
|-----------|-------------|
| `Backtester` | CSV-based historical simulation |
| `ParameterSweep` | Multi-parameter optimization |
| `TradingAlgorithms` | Strategy implementations |

## Data Flow

```
Binance WS ──▶ WebSocket Client ──▶ LockFreeQueue
                                          │
                                          ▼
                                    TickQueue.pop()
                                          │
          ┌───────────────────────────────┼───────────────────────────────┐
          │                               │                               │
          ▼                               ▼                               ▼
   OrderBook.update()              vwap_calc.add_price()           signal_gen.update()
          │                               │                               │
          ▼                               ▼                               ▼
   best_bid/ask                     vwap_, sma_, rsi_              Signal{BUY,SELL,NONE}
```

## Performance Optimizations

### Lock-Free Queue (MPMC)
```cpp
template<typename T, std::size_t Capacity>
class LockFreeQueue {
    // CAS-based push/pop
    // 64-byte cache-line alignment
    // Power-of-2 capacity for fast modulo
};
```

### Ring Buffer
```cpp
template<typename T, std::size_t Capacity>
class RingBuffer {
    // O(1) push/pop
    // No heap allocation
    // Fixed memory footprint
};
```

### Flat Order Book
```cpp
class OrderBook {
    std::array<OrderBookLevel, MAX_LEVELS> bids_;
    std::array<OrderBookLevel, MAX_LEVELS> asks_;
    // No map/tree overhead
    // Cache-friendly sequential access
};
```

## Thread Safety

| Component | Thread Safety | Notes |
|-----------|---------------|-------|
| `LockFreeQueue` | Lock-free | CAS-based, multi-producer/multi-consumer |
| `OrderBook` | Not thread-safe | Single-producer (WS thread) |
| `VWAPCalculator` | Not thread-safe | Single-producer |
| `SignalGenerator` | Not thread-safe | Single-producer |
| `ThreadPool` | Lock-free | Task ring buffer |

**Note**: For multi-threaded use, add `std::atomic` protection or separate instances per thread.

## Configuration

```json
{
  "symbol": "BTCUSDT",
  "sma_period": 20,
  "rsi_period": 14,
  "rsi_oversold": 30.0,
  "rsi_overbought": 70.0,
  "log_level": "info"
}
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| nlohmann/json | 3.x | JSON parsing |
| websocketpp | 0.8.x | WebSocket client |
| Boost.Asio | 1.75+ | Async networking |
| ImGui | 1.89+ | GUI dashboard |
| GLFW | 3.3+ | Window management |
| OpenGL | 3.3+ | GPU rendering |

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./quantsimx
```

## File Structure

```
QuantSimX/
├── include/
│   ├── LockFreeQueue.h     # Lock-free MPMC queue
│   ├── OrderBook.h         # Flat array order book
│   ├── VWAPCalculator.h    # Ring buffer indicators
│   ├── SignalGenerator.h   # Trading signals
│   ├── TradingEngine.h      # Main orchestration
│   └── ...
├── src/
│   ├── main.cpp            # Entry point
│   ├── OrderBook.cpp       # Order book implementation
│   ├── VWAPCalculator.cpp  # Indicator calculations
│   └── ...
├── external/               # Vendored dependencies (ImGui)
├── CMakeLists.txt
└── README.md
```

## Limitations

1. **Single-symbol**: Only supports one trading pair at a time
2. **No order execution**: Simulation only, no real trading
3. **Not production**: For educational/simulation purposes
4. **No persistence**: Data not persisted between sessions

## Future Improvements

- [ ] Multi-symbol support
- [ ] Real order execution via Binance API
- [ ] Database persistence (SQLite/PostgreSQL)
- [ ] Order management system (OMS)
- [ ] Risk management module
- [ ] Performance profiling integration