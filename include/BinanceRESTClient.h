#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class BinanceRESTClient {
public:
    /// Fetch orderbook snapshot from REST API
    /// Returns parsed JSON depth object with "bids" and "asks" fields
    static json fetch_depth_snapshot(const std::string& symbol, int limit = 20);
    
private:
    static const std::string BASE_URL;
};
