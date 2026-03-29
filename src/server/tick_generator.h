#ifndef TICK_GENERATOR_H
#define TICK_GENERATOR_H

#include <vector>
#include <random>
#include <cstdint>
#include "../../src/common/protocol.h"

/**
 * @struct SymbolState
 * @brief Represents the internal market state of a single financial instrument.
 */
struct SymbolState {
    uint32_t id;
    double price;
    double mu;     // Drift (Market direction)
    double sigma;  // Volatility (Standard deviation of returns)
};

/**
 * @class TickGenerator
 * @brief Responsible for calculating price movements using Geometric Brownian Motion (GBM).
 */
class TickGenerator {
public:
    /**
     * @brief Constructor initializes symbols with random prices and volatilities.
     * @param num_symbols The number of unique symbols to simulate.
     */
    explicit TickGenerator(size_t num_symbols);

    /**
     * @brief Computes the next price step and returns a binary MarketMessage.
     * @param symbol_idx The index of the symbol in the internal vector.
     * @param seq The sequence number to assign to this tick.
     * @return MarketMessage A packed binary message for the Feed Handler.
     */
    MarketMessage generate(uint32_t symbol_idx, uint64_t seq);

private:
    std::vector<SymbolState> symbols_;
    
    // Random number generation components
    std::random_device rd_;
    std::mt19937 gen_;
    std::normal_distribution<double> dist_; // For the Wiener process (dW)
};

#endif // TICK_GENERATOR_H