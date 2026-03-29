#include "../../includes/client/symbol_cache.h"
#include <chrono>

SymbolCache::SymbolCache(size_t num_symbols) : cache_(num_symbols) {}

void SymbolCache::updateBid(uint16_t symbol_id, double price, uint32_t qty, uint64_t ts) {
    auto& slot = cache_[symbol_id];
    
    // 1. Increment version to ODD (indicates "Writer is active")
    uint64_t v = slot.version.load(std::memory_order_relaxed);
    slot.version.store(v + 1, std::memory_order_release);

    // 2. Perform Update
    slot.data.best_bid = price;
    slot.data.bid_quantity = qty;
    slot.data.last_update_time = ts;
    slot.data.update_count++;

    // 3. Increment version back to EVEN (indicates "Data is stable")
    slot.version.store(v + 2, std::memory_order_release);
}

void SymbolCache::updateAsk(uint16_t symbol_id, double price, uint32_t qty, uint64_t ts) {
    auto& slot = cache_[symbol_id];
    uint64_t v = slot.version.load(std::memory_order_relaxed);
    slot.version.store(v + 1, std::memory_order_release);

    slot.data.best_ask = price;
    slot.data.ask_quantity = qty;
    slot.data.last_update_time = ts;
    slot.data.update_count++;

    slot.version.store(v + 2, std::memory_order_release);
}

void SymbolCache::updateTrade(uint16_t symbol_id, double price, uint32_t qty, uint64_t ts) {
    auto& slot = cache_[symbol_id];
    uint64_t v = slot.version.load(std::memory_order_relaxed);
    slot.version.store(v + 1, std::memory_order_release);

    slot.data.last_traded_price = price;
    slot.data.last_traded_quantity = qty;
    slot.data.last_update_time = ts;
    slot.data.update_count++;

    slot.version.store(v + 2, std::memory_order_release);
}

bool SymbolCache::getSnapshot(uint16_t symbol_id, MarketState& out_state) const {
    if (symbol_id >= cache_.size()) return false;
    
    const auto& slot = cache_[symbol_id];
    uint64_t v1, v2;

    // Optimistic Concurrency Control loop
    do {
        v1 = slot.version.load(std::memory_order_acquire);
        
        // If v1 is odd, the writer is currently modifying the data. Spin/Wait.
        while (v1 & 1) {
            v1 = slot.version.load(std::memory_order_acquire);
        }

        // Copy the data
        out_state = slot.data;

        // Check if the version changed while we were copying (v2 != v1)
        v2 = slot.version.load(std::memory_order_acquire);
        
    } while (v1 != v2); 

    return true;
}