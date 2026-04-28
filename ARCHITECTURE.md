# QuantSimX Architecture

## Overview

QuantSimX is a high-frequency trading (HFT) simulation platform that provides real-time market data visualization, technical indicator computation, and signal generation for algorithmic trading.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          QuantSimX                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │   Binance    │  │  REST API    │  │  Optimized   │  │   Zero-Copy  │    │
│  │  WebSocket   │──▶│  Snapshot    │──▶│  OrderBook   │──▶│  UDP Ingress │    │
│  └──────────────┘  └──────────────┘  │  (Bitmask)   │  └──────────────┘    │
│         │                              └──────────────┘                      │
│         ▼                                      │                              │
│  ┌──────────────┐                              ▼                              │
│  │     SPSC     │                      ┌──────────────┐                       │
│  │  RingBuffer  │                      │     VWAP     │                       │
│  │  (alignas 64)│                      │  Calculator  │                       │
│  └──────────────┘                      └──────────────┘                       │
│         │                                      │                              │
│         ▼                                      ▼                              │
│  ┌──────────────┐                      ┌──────────────┐                       │
│  │   Signal     │◀─────────────────────│     RSI      │                       │
│  │  Generator   │                      │  (RingBuf)   │                       │
│  └──────────────┘                      └──────────────┘                       │
│         │                                                               │
│         ▼                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  Backtester  │  │    CSV       │  │  Dashboard   │  │  CPU Pinning │    │
│  │              │  │   Writer     │  │   (ImGui)   │  │  (affinity)  │    │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Data Ingestion

| Component | Type | Description |
|-----------|------|-------------|
| `BinanceWebSocketClient` | WebSocket | Connects to Binance streams (depth@100ms, aggTrade) |
| `BinanceRESTClient` | HTTP | Fetches orderbook snapshots |
| `SPSCRingBuffer<TickData, 4096>` | SPSC Queue | Single-producer/single-consumer ring buffer |
| `ZeroCopyUDPIngress` | UDP | Zero-copy network ingress directly from OS socket buffer |

### 2. Market Data Processing

| Component | Data Structure | Complexity |
|-----------|---------------|------------|
| `OrderBook` | Flat array [256 levels] | O(n) insert, O(1) best bid/ask |
| `OptimizedOrderBook` | 64-bit bitmask + 32B packed Order | O(1) BBO via bit-scanning |
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
Binance WS ──▶ WebSocket Client ──▶ SPSCRingBuffer (SPSC)
                                           │
                                           ▼
                              ZeroCopyUDPIngress (optional)
                                           │
                                           ▼
                                     TickQueue.pop()
                                           │
           ┌───────────────────────────────┼───────────────────────────────┐
           │                               │                               │
           ▼                               ▼                               ▼
    OptimizedOrderBook.update()    vwap_calc.add_price()           signal_gen.update()
           │                               │                               │
           ▼                               ▼                               ▼
    __builtin_ctzll(mask)            vwap_, sma_, rsi_              Signal{BUY,SELL,NONE}
    O(1) Best Bid/Ask
```

## Performance Optimizations

All HFT optimizations are consolidated in `include/HFTPerf.h`:

### Zero-Copy UDP Ingress
- Network payloads parsed directly from OS socket buffer into `MarketData` struct
- `alignas(64)` prevents false sharing across CPU cores

### Lock-Free SPSC Ring Buffer
```cpp
template<typename T, std::size_t Capacity>
class alignas(64) SPSCRingBuffer { ... };
```
- Single-producer/single-consumer, no CAS needed
- `alignas(64)` hardware destructive interference size

### O(1) Hardware Bit-Scanning OrderBook
```cpp
class BitmaskOrderBook {
    uint64_t bid_mask_, ask_mask_;  // 64-bit bitmask of active levels
    int best_bid_level() const { return __builtin_ctzll(bid_mask_); }
};
```
- Finding BBO uses `__builtin_ctzll()` - single clock cycle

### L1 Cache-Line Packing
```cpp
struct PackedOrder { ... };  // 24 bytes, 2 fit in 64B L1 line
```

### Hash-Free Direct Memory Access
- `PackedOrder[64]` flat arrays replace `std::unordered_map`
- O(1) pointer arithmetic lookup, no hash collisions

### OS CPU Pinning
```cpp
CpuPinner::pin(core_id);  // Locks thread via pthread_setaffinity_np
```

## Thread Safety

| Component | Thread Safety | Notes |
|-----------|---------------|-------|
| `SPSCRingBuffer` | Lock-free SPSC | Single-producer/single-consumer, no CAS |
| `BitmaskOrderBook` | Not thread-safe | Single-producer (network thread) |
| `ZeroCopyUDP` | OS-level | Direct socket buffer parsing |
| `LockFreeQueue` | Lock-free MPMC | CAS-based, multi-producer/multi-consumer |
| `CpuPinner` | N/A | Compile-time thread affinity |

**Note**: `CpuPinner::pin()` locks threads to isolated CPU cores to prevent migration overhead.

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
│   ├── HFTPerf.h           # HFT optimizations (SPSC, bitmask BBO, CPU pin)
│   ├── LatencyProfiler.h   # Cycle-accurate latency tracking
│   ├── RiskManager.h       # Position limits, daily loss limits
│   ├── SymbolIndex.h       # Thread-safe multi-symbol map
│   ├── Signal.h            # Signal enum, TickData struct
│   ├── LockFreeQueue.h     # MPMC lock-free queue
│   ├── OrderBook.h         # Flat array order book
│   ├── VWAPCalculator.h    # Ring buffer indicators
│   ├── SignalGenerator.h   # Trading signals
│   ├── TradingEngine.h     # Main orchestration
│   └── ...
├── src/
│   ├── main.cpp
│   ├── OrderBook.cpp
│   └── ...
├── tests/
│   └── test_hft_perf.cpp   # Performance benchmarks
├── external/               # Vendored dependencies (ImGui)
├── CMakeLists.txt
└── README.md
```

## Code Quality

- **Single header for Signal enum** - `Signal.h` defines `Signal`, `SignalEvent`, `TickData`
- **Fixed-point arithmetic** - `RiskManager` uses cents to avoid floating-point issues
- **Thread-safe registry** - `SymbolIndex<T>` with mutex protection for multi-symbol support
- **Cycle-accurate profiling** - `LatencyProfiler` uses `__builtin_readcyclecounter()`

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