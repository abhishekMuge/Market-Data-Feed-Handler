#include "tick_generator.h"
#include <cmath>

TickGenerator::TickGenerator(size_t num_symbols) 
    : gen_(rd_()), dist_(0.0, 1.0) {
    
    std::uniform_real_distribution<double> p_dist(100.0, 5000.0);
    std::uniform_real_distribution<double> s_dist(0.01, 0.06);
    
    for (uint32_t i = 0; i < num_symbols; ++i) {
        symbols_.push_back({i, p_dist(gen_), 0.0, s_dist(gen_)});
    }
}

MarketMessage TickGenerator::generate(uint32_t symbol_idx, uint64_t seq) {
    auto& s = symbols_[symbol_idx];
    
    // GBM Formula: dS = mu * S * dt + sigma * S * dW
    const double dt = 0.001; // 1ms time step
    double dW = dist_(gen_); // Standard Normal Distribution
    
    double drift = s.mu * s.price * dt;
    double diffusion = s.sigma * s.price * dW * std::sqrt(dt);
    
    s.price += (drift + diffusion);
    if (s.price < 1.0) s.price = 1.0; // Price floor

    MarketMessage msg{};
    msg.seq_num = seq;
    msg.symbol_id = s.id;
    
    // 70% Quote vs 30% Trade logic
    if (rand() % 100 < 70) {
        msg.type = MsgType::Quote;
        double spread = s.price * 0.001; 
        msg.data.quote.bid_price = s.price - (spread / 2);
        msg.data.quote.ask_price = s.price + (spread / 2);
        msg.data.quote.bid_qty = (rand() % 10 + 1) * 100;
        msg.data.quote.ask_qty = (rand() % 10 + 1) * 100;
    } else {
        msg.type = MsgType::Trade;
        msg.data.trade.price = s.price;
        msg.data.trade.qty = (rand() % 5 + 1) * 50;
    }
    return msg;
}