#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

enum class MsgType : uint8_t { Quote = 1, Trade = 2 };

#pragma pack(push, 1)
struct MarketMessage {
    uint64_t seq_num;
    uint64_t timestamp_ns;
    uint32_t symbol_id;
    MsgType type;
    
    union {
        struct {
            double bid_price;
            double ask_price;
            uint32_t bid_qty;
            uint32_t ask_qty;
        } quote;
        struct {
            double price;
            uint32_t qty;
        } trade;
    } data;
    uint32_t checksum;
};
#pragma pack(pop)

inline uint32_t calculate_checksum(const MarketMessage& msg) {
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&msg);
    uint32_t xor_sum = 0;
    
    // XOR every byte except the last 4 bytes (the checksum itself)
    for (std::size_t i = 0; i < sizeof(MarketMessage) - 4; ++i) {
        xor_sum ^= raw[i];
    }
    return xor_sum;
}

#endif