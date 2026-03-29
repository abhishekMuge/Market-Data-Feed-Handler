#include "../../includes/client/binary_parser.h"
#include <iostream>
#include <cstring>

#include "binary_parser.h"

BinaryParser::BinaryParser(SymbolCache& cache, LatencyTracker& tracker) : 
    cache_(cache), latency_tracker_(tracker), write_pos_(0), expected_seq_(1), first_msg_(true) {
    std::memset(staging_buffer_, 0, STAGING_SIZE);
}

void BinaryParser::reset() {
    write_pos_ = 0;
    expected_seq_ = 1; // Or whatever your starting seq is
    first_msg_ = true;
}

// size_t BinaryParser::parse(const uint8_t* data, size_t len) {
//     // 1. Safety check for buffer overflow
//     if (write_pos_ + len > STAGING_SIZE) {
//         // std::cerr << "[ERROR] Staging buffer overflow! Clearing buffer.\n";
//         write_pos_ = 0;
//         return 0;
//     }

//     // 2. Append new data to the end of our current fragment
//     std::memcpy(staging_buffer_ + write_pos_, data, len);
//     write_pos_ += len;

//     size_t processed_offset = 0;
//     size_t msg_count = 0;

//     // 3. Hot Path: Process as many whole messages as possible
//     while (write_pos_ - processed_offset >= sizeof(MarketMessage)) {
//         // ZERO-COPY: Treat the memory address as a struct pointer
//         const MarketMessage* msg = reinterpret_cast<const MarketMessage*>(staging_buffer_ + processed_offset);

//         // A. Checksum Validation
//         if (validate_checksum(msg)) {
//             // B. Sequence Gap Detection

//             if (!first_msg_ && msg->seq_num != expected_seq_) {
                
//                 // std::cerr << "[WARN] Sequence Gap! Expected: " << expected_seq_ 
//                         //   << " Received: " << msg->seq_num << "\n";
//             }
            
//             first_msg_ = false;
//             expected_seq_ = msg->seq_num + 1;

//             // C. Business Logic
//             handle_message(msg);
//             msg_count++;
//         } else {
//             static int error_count = 0;
//             if (++error_count % 1000 == 1) {
//                 std::cerr << "[ERROR] Checksum failed (suppressing next 1000)\n";
//             }
//             // std::cerr << "[ERROR] Malformed Message: Checksum failed for Seq " << msg->seq_num << "\n";
//             // In a real HFT app, you might increment the offset by only 1 byte 
//             // to re-sync, but here we assume fixed-length protocol stability.
//         }

//         processed_offset += sizeof(MarketMessage);
//     }

//     // 4. Fragment Handling: Move leftover bytes to the start of the buffer
//     size_t remaining = write_pos_ - processed_offset;
//     if (remaining > 0 && processed_offset > 0) {
//         std::memmove(staging_buffer_, staging_buffer_ + processed_offset, remaining);
//     }
//     write_pos_ = remaining;

//     return msg_count;
// }
size_t BinaryParser::parse(const uint8_t* data, size_t len) {
    // 1. Safety check
    if (write_pos_ + len > STAGING_SIZE) {
        write_pos_ = 0; // Buffer is trashed, reset
        return 0;
    }

    // 2. Append data
    std::memcpy(staging_buffer_ + write_pos_, data, len);
    write_pos_ += len;

    const size_t MSG_SIZE = sizeof(MarketMessage);
    size_t processed_offset = 0;
    size_t msg_count = 0;
    size_t gap_count_ = 0;
    // 3. Hot Path
    // Using a pointer-based loop is slightly faster than index math
    while (processed_offset + MSG_SIZE <= write_pos_) {
        const MarketMessage* msg = reinterpret_cast<const MarketMessage*>(staging_buffer_ + processed_offset);

        if (__builtin_expect(validate_checksum(msg), 1)) { // Branch prediction hint: usually true
            // Sequence Gap Logic
            if (!first_msg_ && msg->seq_num != expected_seq_) {
                // Handle Gap: update a counter, don't std::cerr here
                gap_count_++;
            }
            
            first_msg_ = false;
            expected_seq_ = msg->seq_num + 1;

            handle_message(msg);
            msg_count++;
            
            // Advance by full message size
            processed_offset += MSG_SIZE;
        } else {
            // --- CRITICAL CHANGE ---
            // If checksum fails, we have lost "Framing". 
            // Jumping by MSG_SIZE here is dangerous. 
            static int error_count = 0;
            if (++error_count % 1000 == 1) {
                std::cerr << "[ERROR] Sync Lost at Seq " << msg->seq_num << "\n";
            }

            // Option A: Stop and wait for reconnect (Safest for HFT)
            // Option B: Skip 1 byte and try to find a valid message (The "Search" method)
            processed_offset += 1; 
        }
    }

    // 4. Move leftovers
    size_t remaining = write_pos_ - processed_offset;
    if (remaining > 0) {
        // Only move if we actually processed something
        if (processed_offset > 0) {
            std::memmove(staging_buffer_, staging_buffer_ + processed_offset, remaining);
        }
    }
    write_pos_ = remaining;

    return msg_count;
}

bool BinaryParser::validate_checksum(const MarketMessage* msg) {
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(msg);
    uint32_t xor_result = 0;

    // XOR all bytes except the last 4 (the checksum itself)
    for (size_t i = 0; i < sizeof(MarketMessage) - 4; ++i) {
        xor_result ^= raw[i];
    }

    // Compare calculated XOR with the checksum stored at the end of the struct
    // Note: This assumes your MarketMessage struct has a 'uint32_t checksum' at the end.
    return xor_result == msg->checksum;
}

void BinaryParser::handle_message(const MarketMessage* msg) {
    // 1. Capture "Arrival Time" immediately (The Hardware/Software Latency)
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t arrival_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    // 2. Calculate the Delta
    // Note: This assumes your Server and Client clocks are synchronized (PTP/NTP)
    // On localhost, this works perfectly.
    if (__builtin_expect(arrival_ns > msg->timestamp_ns, 1)) {
        uint64_t latency = arrival_ns - msg->timestamp_ns;
        latency_tracker_.record(latency);
    }
    // Determine the type and call the corresponding lock-free update
    switch (msg->type) {
        case MsgType::Quote:
            // Updating both sides of the book from a single Quote message
            cache_.updateBid(msg->symbol_id, 
                             msg->data.quote.bid_price, 
                             0, // If protocol adds bid_qty, put it here
                             msg->timestamp_ns);
            
            cache_.updateAsk(msg->symbol_id, 
                             msg->data.quote.ask_price, 
                             0, 
                             msg->timestamp_ns);
            break;

        case MsgType::Trade:
            cache_.updateTrade(msg->symbol_id, 
                               msg->data.trade.price, 
                               msg->data.trade.qty, 
                               msg->timestamp_ns);
            break;

        default:
            // Handle unknown message types gracefully
            break;
    }
}