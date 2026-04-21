#pragma once

#include "Signal.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <deque>

class TradingAlgorithm {
public:
    virtual ~TradingAlgorithm() = default;
    virtual Signal update(const TickData& tick) = 0;
    virtual const char* name() const = 0;
    virtual const char* description() const = 0;
    virtual void reset() = 0;
    virtual void configure(const std::string& params) {}
    
    const std::vector<SignalEvent>& get_signals() const { return signals_; }
    const SignalEvent& last_signal() const { return last_signal_; }
    void clear_signals() { signals_.clear(); }

protected:
    void emit_signal(Signal sig, const std::string& reason, double aux = 0.0) {
        if (sig != Signal::NONE) {
            SignalEvent evt;
            evt.timestamp_ms = last_tick_.timestamp_ms;
            evt.signal = sig;
            evt.reason = reason;
            evt.price = last_tick_.price;
            evt.auxiliary = aux;
            signals_.push_back(evt);
            last_signal_ = evt;
        }
    }
    
    TickData last_tick_;
    SignalEvent last_signal_;
    std::vector<SignalEvent> signals_;
};

using AlgorithmFactory = std::function<std::unique_ptr<TradingAlgorithm>()>;

class AlgorithmRegistry {
public:
    static AlgorithmRegistry& instance() {
        static AlgorithmRegistry reg;
        return reg;
    }
    
    void register_algorithm(const std::string& name, AlgorithmFactory factory) {
        factories_[name] = std::move(factory);
    }
    
    std::unique_ptr<TradingAlgorithm> create(const std::string& name) const {
        auto it = factories_.find(name);
        if (it != factories_.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    std::vector<std::string> available() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }

private:
    std::unordered_map<std::string, AlgorithmFactory> factories_;
};

#define REGISTER_ALGORITHM(Name, ClassName) \
    namespace { \
        struct ClassName##Registrar { \
            ClassName##Registrar() { \
                AlgorithmRegistry::instance().register_algorithm( \
                    Name, \
                    []() { return std::make_unique<ClassName>(); } \
                ); \
            } \
        }; \
        static ClassName##Registrar s_##ClassName##_registrar; \
    }
