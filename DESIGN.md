# QuantSimX Design Document

## Project Overview

**QuantSimX** is a high-frequency trading (HFT) simulation and visualization platform that streams real-time market data from Binance, computes technical indicators, generates trading signals, and provides backtesting capabilities.

### Key Features
- Real-time order book visualization
- VWAP, SMA, RSI indicator computation
- Trading signal generation based on RSI + VWAP crossover
- Historical backtesting with CSV data
- ImGui-based professional trading dashboard

## Technical Specifications

### Performance Targets
- Lock-free tick processing (< 1μs latency)
- Order book update < 100ns
- Indicator computation < 500ns
- UI rendering 60 FPS target

### Data Flow
```
Binance WebSocket (depth@100ms, aggTrade)
    ↓
BinanceWebSocketClient
    ↓
LockFreeQueue (MPMC, 4096 capacity)
    ↓
TradingEngine (process_pending_ticks)
    ↓
┌──────────────────────────────────────────┐
│  OrderBook    │  VWAPCalculator  │  SignalGenerator  │
│  (flat array) │  (ring buffer)   │  (state machine)  │
└──────────────────────────────────────────┘
    ↓
Backtester / Dashboard
```

## Indicator Specifications

### VWAP (Volume Weighted Average Price)
```cpp
cumulative_volume_price_ += price * volume
cumulative_volume_ += volume
vwap = cumulative_volume_price_ / cumulative_volume
```
- **Type**: Cumulative, O(1) update
- **Memory**: RingBuffer[4096] for recent samples

### SMA (Simple Moving Average)
- **Period**: 20 (configurable)
- **Calculation**: Sum of last N prices / N
- **Memory**: RingBuffer[4096]

### RSI (Relative Strength Index)
- **Period**: 14 (configurable)
- **Formula**: 100 - (100 / (1 + RS))
- **RS**: Average gain / Average loss over period
- **Thresholds**: 
  - Oversold: < 30
  - Overbought: > 70

## Signal Generation Logic

```cpp
Signal update(timestamp, price, vwap, rsi):
    price_above = price > vwap
    rsi_oversold = rsi < 30
    rsi_overbought = rsi > 70
    
    // BUY conditions
    if (price_above && !prev_price_above):
        return BUY  // Price crossed above VWAP
    if (rsi > 30 && was_rsi_oversold):
        return BUY  // RSI exited oversold
    
    // SELL conditions
    if (!price_above && prev_price_above):
        return SELL // Price crossed below VWAP
    if (rsi < 70 && was_rsi_overbought):
        return SELL // RSI exited overbought
    
    return NONE
```

## Order Book Specification

### Data Structure
```cpp
class OrderBook {
    std::array<OrderBookLevel, 256> bids_;  // Sorted descending
    std::array<OrderBookLevel, 256> asks_;  // Sorted ascending
    std::size_t bid_count_;
    std::size_t ask_count_;
}
```

### Operations
| Operation | Complexity | Notes |
|-----------|------------|-------|
| get_best_bid() | O(1) | bids_[0].price |
| get_best_ask() | O(1) | asks_[0].price |
| insert_bid() | O(n) | Linear search + shift |
| erase_bid() | O(n) | Linear search + shift |

## Lock-Free Queue Specification

### MPMC Queue (Multi-Producer Multi-Consumer)
```cpp
template<typename T, std::size_t Capacity>
class LockFreeQueue {
    // Power-of-2 capacity for fast modulo
    // 64-byte cache-line alignment
    // Version-based CAS synchronization
}
```

### Invariants
- Capacity must be power of 2
- T must be trivially destructible
- No allocation after construction

## Configuration Schema

```json
{
  "symbol": "BTCUSDT",
  "sma_period": 20,
  "rsi_period": 14,
  "rsi_oversold": 30.0,
  "rsi_overbought": 70.0,
  "websocket_reconnect_delay_ms": 1000,
  "max_orderbook_levels": 256
}
```

## Error Handling

| Error | Handling |
|-------|-----------|
| WebSocket disconnect | Auto-reconnect with exponential backoff |
| Invalid JSON | Log error, skip message |
| Queue full | Drop oldest tick, log warning |
| Order book empty | Return 0.0 for bid/ask |

## Testing Strategy

### Unit Tests
- LockFreeQueue: concurrent push/pop verification
- OrderBook: insert/erase correctness
- VWAPCalculator: indicator accuracy
- SignalGenerator: signal generation logic

### Integration Tests
- WebSocket connection lifecycle
- End-to-end tick processing
- Backtester CSV replay

### Performance Tests
- Tick processing latency (< 1μs target)
- UI frame time (< 16.67ms for 60 FPS)
- Memory footprint verification

## Risk Disclaimers

1. **Simulation Only**: No real money is traded
2. **No Order Execution**: Orders are not sent to exchange
3. **Backtest Limitations**: Past performance does not guarantee future results
4. **Data Accuracy**: Market data from Binance may have latency

## Dependencies

| Library | Version | License |
|---------|---------|---------|
| nlohmann/json | 3.10+ | MIT |
| websocketpp | 0.8+ | BSD |
| Boost.Asio | 1.75+ | Boost |
| ImGui | 1.89+ | MIT |
| GLFW | 3.3+ | zlib |
| OpenGL | 3.3+ | Khronos |

## Build Requirements

- C++20 compiler (GCC 11+, Clang 14+)
- CMake 3.20+
- 2GB RAM minimum
- Linux/macOS/Windows