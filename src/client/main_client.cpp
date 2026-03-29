#include "../../includes/client/binary_parser.h"
#include "../../includes/client/market_data_socket.h"
#include "../../includes/client/symbol_cache.h"
#include <iostream>
#include <vector>
#include <numeric>
#include <thread>
#include <cstring>

class FeedProcessor {
public:
    FeedProcessor(const std::string& host, uint16_t port)
        : socket_(),
          cache_(1000),
        // Support up to 1000 symbols
          tracker_(1000000),
          parser_(cache_, tracker_),      // Inject cache into parser
          host_(host),
          port_(port) {}

    // void run() {
    //     // 1. Initial Connection
    //     if (!socket_.connect(host_, port_)) {
    //         std::cerr << "[ERROR] Could not connect to Exchange Simulator.\n";
    //         return;
    //     }

    //     // 2. Subscribe to 500 symbols as per requirements
    //     std::vector<uint16_t> symbols(500);
    //     std::iota(symbols.begin(), symbols.end(), 1); // IDs 1 to 500
    //     socket_.send_subscription(symbols);

    //     // std::cout << "[INFO] Subscription sent. Starting Feed Loop...\n";

    //     // 3. The Hot Path Loop
    //     uint8_t buffer[65536]; // 64KB buffer for high throughput
    //     while (running_) {
    //         ssize_t bytes = socket_.receive(buffer, sizeof(buffer));

    //         if (bytes > 0) {
    //             // This call: Socket Bytes -> Parser -> Symbol Cache updates
    //             parser_.parse(buffer, bytes);
    //         } else if (bytes < 0) {
    //             // TCP FIN received - Server closed the connection!
    //             std::cerr << "[SYSTEM] Connection lost. Attempting reconnect..." << std::endl;
    //             socket_.connect(host_, port_); 
    //             // Re-subscribe after reconnect
    //             std::vector<uint16_t> symbols(500);
    //             std::iota(symbols.begin(), symbols.end(), 1);
    //             socket_.send_subscription(symbols);
    //         } else {
    //             // No data: yield to prevent 100% CPU on idle (optional for HFT)
    //             // std::this_thread::yield();
    //         }
    //     }
    // }
    void run() {
        // 1. Initial Connection & Subscription
        if (!socket_.connect(host_, port_)) {
            std::cerr << "[ERROR] Could not connect.\n";
            return;
        }

        // Pre-allocate subscription vector once to avoid heap allocations
        std::vector<uint16_t> symbols(500);
        std::iota(symbols.begin(), symbols.end(), 1);
        socket_.send_subscription(symbols);

        uint8_t buffer[65536]; // 64KB stack buffer is perfect
        
        while (running_) {
            ssize_t bytes = socket_.receive(buffer, sizeof(buffer));

            if (bytes > 0) {
                // SUCCESS: Drain the data into the parser immediately
                parser_.parse(buffer, static_cast<size_t>(bytes));
            } 
            // else if (bytes == 0) {
            //     // DISCONNECT: Server closed the socket (FIN packet)
            //     std::cerr << "[SYSTEM] Server closed connection. Reconnecting..." << std::endl;
                
            //     // std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Don't spam reconnect
            //     // std::cerr << "[SYSTEM] Disconnected. Cleaning up...\n";
            //     // parser_.reset(); // CRITICAL: Clear the staging buffer -- this failed not solved
            //     if (socket_.connect(host_, port_)) {
            //         socket_.send_subscription(symbols);
            //     }
            // } 
            else {
                // bytes < 0: Check if it's a real error or just empty buffer
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Buffer is empty. In HFT, we usually just loop back immediately.
                    // Optionally: _mm_pause(); // Hint to CPU we are in a spin-loop
                    continue; 
                } else {
                    // A real socket error occurred (e.g., Connection Reset)
                    std::cerr << "[ERROR] Socket error: " << strerror(errno) << std::endl;
                    socket_.connect(host_, port_);
                    socket_.send_subscription(symbols);
                }
            }
        }
    }
    // Allow other threads to view the market state
    const SymbolCache& get_cache() const { return cache_; }
    const LatencyTracker& get_tracker() const {return tracker_;}

private:
    MarketDataSocket socket_;
    SymbolCache cache_;
    LatencyTracker tracker_;
    BinaryParser parser_;
    std::string host_;
    uint16_t port_;
    bool running_ = true;
};

int main() {
    // Start the Feed Processor
    FeedProcessor processor("127.0.0.1", 8881);
    
    // Launch the processor in its own thread (The "One Writer Thread")
    std::thread feed_thread(&FeedProcessor::run, &processor);
    std::cout << "PROTOCOL CHECK: MarketMessage size is " << sizeof(MarketMessage) << " bytes." << std::endl;
    // Demonstration: Use the "Main Thread" as a Reader
    while (true) 
    // for(int i = 0; i < 100000; i++)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        MarketState apple_state;
        if (processor.get_cache().getSnapshot(42, apple_state)) {
            std::cout << "--- Symbol 42 Snapshot ---" << "\n"
                      << "Bid: " << apple_state.best_bid << " | Ask: " << apple_state.best_ask << "\n"
                      << "Last Trade: " << apple_state.last_traded_price << "\n"
                      << "Update Count: " << apple_state.update_count << "\n"
                      << "--------------------------" << std::endl;
        }

        // // Get stats from the tracker
        // auto stats = processor.get_tracker().get_stats();
        // if (stats.sample_count > 0) {
        //     std::cout << "--- Performance Metrics ---" << "\n"
        //               << "Samples: " << stats.sample_count << "\n"
        //               << "Mean:    " << stats.mean << " ns\n"
        //               << "P99:     " << stats.p99 << " ns\n"
        //               << "P99.9:   " << stats.p999 << " ns\n"
        //               << "--------------------------" << std::endl;
        // }

        // // if (i > 0 && i % 10 == 0) 
        // {
        //     processor.get_tracker().export_to_csv("latency_report.csv");
        // }
    }
    // processor.get_tracker().export_to_csv("latency_report.csv");
    // processor->get_tracker->export_to_csv("latency_report.csv");
    if (feed_thread.joinable()) feed_thread.join();
    return 0;
}