#include "ParameterSweep.h"
#include <iostream>
#include <algorithm>

ParameterSweep::ParameterSweep(ThreadPool& pool) : pool_(pool) {}

void ParameterSweep::run_single(ParameterSet params, const std::string& csv_path) {
    Backtester bt(params.rsi_oversold, params.rsi_overbought);
    if (!bt.load_csv(csv_path)) {
        std::cerr << "[ParameterSweep] Failed to load CSV\n";
        return;
    }
    
    auto metrics = bt.run();
    
    std::lock_guard<std::mutex> lock(result_mutex_);
    result_.all_results.emplace_back(params, metrics);
    
    if (result_.best_metrics.total_trades == 0 || 
        metrics.sharpe_ratio > result_.best_metrics.sharpe_ratio) {
        result_.best_params = params;
        result_.best_metrics = metrics;
    }
}

OptimizationResult ParameterSweep::run(const std::string& csv_path,
                                       const std::vector<double>& oversold_values,
                                       const std::vector<double>& overbought_values) {
    result_ = OptimizationResult();
    
    for (double oversold : oversold_values) {
        for (double overbought : overbought_values) {
            if (oversold >= overbought) continue;
            
            ParameterSet params;
            params.rsi_oversold = oversold;
            params.rsi_overbought = overbought;
            
            pool_.enqueue([this, params, csv_path] {
                run_single(params, csv_path);
            });
        }
    }
    
    pool_.wait_all();
    return result_;
}
