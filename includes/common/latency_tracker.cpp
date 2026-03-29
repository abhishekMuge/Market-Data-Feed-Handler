#include "latency_tracker.h"

LatencyTracker::LatencyStats LatencyTracker::get_stats() const {
    LatencyStats stats{};
    uint64_t count = total_count_.load();
    if (count == 0) return stats;

    stats.sample_count = count;
    uint64_t sum = 0;
    uint64_t min_val = UINT64_MAX;
    uint64_t max_val = 0;

    // Calculate Min, Max, Mean from the actual samples
    size_t search_limit = std::min((size_t)count, max_samples_);
    for (size_t i = 0; i < search_limit; ++i) {
        uint64_t val = samples_[i].load();
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
        sum += val;
    }

    stats.min = min_val;
    stats.max = max_val;
    stats.mean = sum / search_limit;

    // Calculate Percentiles from Histogram Buckets
    auto get_p = [&](double percentile) {
        uint64_t target = static_cast<uint64_t>(count * percentile);
        uint64_t current_sum = 0;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            current_sum += buckets_[i].load();
            if (current_sum >= target) return i * 10; // Bucket resolution is 10ns
        }
        return (size_t)10000;
    };

    stats.p50 = get_p(0.50);
    stats.p95 = get_p(0.95);
    stats.p99 = get_p(0.99);
    stats.p999 = get_p(0.999);

    return stats;
}

void LatencyTracker::export_to_csv(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open " << filename << " for writing.\n";
        return;
    }

    // Header
    file << "Latency_Bucket_Start_NS,Packet_Count\n";

    for (size_t i = 0; i < buckets_.size(); ++i) {
        uint64_t val = buckets_[i].load(std::memory_order_relaxed);
        // Only export buckets that have data to keep the file clean
        if (val > 0) {
            file << (i * 10) << "," << val << "\n";
        }
    }

    file.close();
    std::cout << "[INFO] Latency histogram exported to " << filename << std::endl;
}