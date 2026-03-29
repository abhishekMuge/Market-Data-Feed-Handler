#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include "exchange_simulator.h"

// Atomic flag to handle graceful shutdown across threads
std::atomic<bool> keep_running(true);

// Signal handler for Ctrl+C (SIGINT) and Kill signals (SIGTERM)
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[SYSTEM] Shutdown signal received. Cleaning up resources...\n";
        keep_running = false;
    }
}

int main(int argc, char* argv[]) {
    // 1. Register Signal Handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 2. Configuration Parameters
    // In a real HFT environment, these might be loaded from a config.json or CLI args
    const uint16_t PORT = 8881;
    const size_t NUM_SYMBOLS = 100;
    const uint32_t TICK_RATE_HZ = 500000; // 100k msgs/sec for initial testing

    std::cout << "==========================================\n";
    std::cout << "   HIGH-PERFORMANCE EXCHANGE SIMULATOR    \n";
    std::cout << "==========================================\n";
    std::cout << "[INFO] Port: " << PORT << "\n";
    std::cout << "[INFO] Symbols: " << NUM_SYMBOLS << "\n";
    std::cout << "[INFO] Target Rate: " << TICK_RATE_HZ << " ticks/sec\n";
    std::cout << "------------------------------------------\n";

    std::cout << "PROTOCOL CHECK: MarketMessage size is " << sizeof(MarketMessage) << " bytes." << std::endl;

    try {
        // 3. Initialize the Simulator
        ExchangeSimulator exchange(PORT, NUM_SYMBOLS);

        // 4. Set High-Performance Constraints
        exchange.set_tick_rate(TICK_RATE_HZ);
        exchange.enable_fault_injection(false); // Enable this later to test client recovery

        // 5. Start the Listener Thread
        exchange.start();

        // 6. Execution Loop
        // Note: For ultra-low latency, you would typically use pthread_setaffinity_np
        // to pin this thread to a specific isolated CPU core.
        std::cout << "[STATUS] Server is live. Waiting for Feed Handlers...\n";
        
        // We run the simulation logic. The 'keep_running' flag should be 
        // checked inside the exchange.run() loop for a clean exit.
        exchange.run();

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[SYSTEM] Exchange Simulator shut down successfully.\n";
    return 0;
}
// cat < /dev/tcp/127.0.0.1/9000 | hexdump -C