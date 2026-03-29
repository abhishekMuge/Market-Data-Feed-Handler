#ifndef BINARY_PARSER_H
#define BINARY_PARSER_H

#include "../common/protocol.h"
#include "../../includes/common/latency_tracker.h"
#include "symbol_cache.h"
#include <cstdint>
#include <vector>
#include <chrono>

class BinaryParser {
public:
    BinaryParser(SymbolCache& cache, LatencyTracker& tracker);

    void reset();

    // Processes raw bytes from the socket. 
    // Returns number of full messages processed.
    size_t parse(const uint8_t* data, size_t len);

private:
    bool validate_checksum(const MarketMessage* msg);
    void handle_message(const MarketMessage* msg);

    // Staging area for fragmented packets
    static constexpr size_t STAGING_SIZE = 65536;
    uint8_t staging_buffer_[STAGING_SIZE];
    size_t write_pos_ = 0;

    SymbolCache& cache_;
    LatencyTracker& latency_tracker_;
    uint64_t expected_seq_ = 1;
    bool first_msg_ = true;
};

#endif