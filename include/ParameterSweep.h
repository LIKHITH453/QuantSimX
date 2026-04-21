#ifndef PARAMETER_SWEEP_H
#define PARAMETER_SWEEP_H

#include "ThreadPool.h"
#include "Backtester.h"
#include <vector>
#include <utility>
#include <map>
#include <mutex>

struct ParameterSet {
    double rsi_oversold = 30.0;
    double rsi_overbought = 70.0;
    double vwap_sma_period = 20.0;
};

struct OptimizationResult {
    ParameterSet best_params;
    BacktestMetrics best_metrics;
    std::vector<std::pair<ParameterSet, BacktestMetrics>> all_results;
};

class ParameterSweep {
public:
    explicit ParameterSweep(ThreadPool& pool);
    
    OptimizationResult run(const std::string& csv_path,
                          const std::vector<double>& oversold_values,
                          const std::vector<double>& overbought_values);
    
private:
    void run_single(ParameterSet params, const std::string& csv_path);
    
    ThreadPool& pool_;
    OptimizationResult result_;
    std::mutex result_mutex_;
};

#endif
