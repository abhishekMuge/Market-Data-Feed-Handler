#ifndef SYMBOL_CACHE_H
#define SYMBOL_CACHE_H

#include <atomic>
#include <vector>
#include <cstdint>

struct MarketState {
    double best_bid = 0.0;
    double best_ask = 0.0;
    uint32_t bid_quantity = 0;
    uint32_t ask_quantity = 0;
    double last_traded_price = 0.0;
    uint32_t last_traded_quantity = 0;
    uint64_t last_update_time = 0;
    uint64_t update_count = 0;
};

class SymbolCache {
public:
    explicit SymbolCache(size_t num_symbols = 1000);

    // Writer API (Feed Handler)
    void updateBid(uint16_t symbol_id, double price, uint32_t qty, uint64_t ts);
    void updateAsk(uint16_t symbol_id, double price, uint32_t qty, uint64_t ts);
    void updateTrade(uint16_t symbol_id, double price, uint32_t qty, uint64_t ts);

    // Reader API (Execution/UI)
    bool getSnapshot(uint16_t symbol_id, MarketState& out_state) const;

private:
    struct InternalState {
        // The version counter: Even = stable, Odd = being updated
        std::atomic<uint64_t> version{0}; 
        MarketState data;
    };

    std::vector<InternalState> cache_;
};

#endif