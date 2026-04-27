#pragma once

#include <chrono>
#include <cstdint>
#include <vector>
#include <limits>

class LatencyProfiler {
public:
    struct Record {
        uint64_t timestamp_ns;
        const char* stage;
        uint64_t cycles;
    };

    LatencyProfiler() : enabled_(false) {
        records_.reserve(1024);
    }

    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool is_enabled() const { return enabled_; }

    void mark(const char* stage) {
        if (!enabled_) return;
        Record r;
        r.timestamp_ns = get_time_ns();
        r.stage = stage;
        r.cycles = get_cycles();
        records_.push_back(r);
    }

    void mark_with_latency(const char* stage, uint64_t start_ns) {
        if (!enabled_) return;
        Record r;
        r.timestamp_ns = get_time_ns();
        r.stage = stage;
        r.cycles = get_cycles() - start_ns;
        records_.push_back(r);
    }

    void clear() { records_.clear(); }

    const std::vector<Record>& records() const { return records_; }

    uint64_t total_cycles() const {
        if (records_.size() < 2) return 0;
        return records_.back().cycles - records_.front().cycles;
    }

    double total_ms() const { return cycles_to_ms(total_cycles()); }

    void print_summary() const;

private:
    static inline uint64_t get_time_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    static inline uint64_t get_cycles() {
        return __builtin_readcyclecounter();
    }

    static inline double cycles_to_ms(uint64_t cycles) {
        return static_cast<double>(cycles) / 2800000.0;
    }

    bool enabled_;
    std::vector<Record> records_;
};