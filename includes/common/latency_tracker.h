#include <vector>
#include <atomic>
#include <string>
#include <fstream>
#include <iostream>

class LatencyTracker {
public:
    struct LatencyStats {
        uint64_t min, max, mean;
        uint64_t p50, p95, p99, p999;
        uint64_t sample_count;
    };

    // Constructor: 1000 buckets, each 10ns wide (covers 0-10,000ns)
    LatencyTracker(size_t max_samples = 1000000) 
        : max_samples_(max_samples), 
          samples_(max_samples), 
          buckets_(1001) { 
        for(auto& b : buckets_) b.store(0);
    }

    inline void record(uint64_t latency_ns) {
        // 1. Store in Ring Buffer (Thread-safe index)
        size_t idx = index_.fetch_add(1, std::memory_order_relaxed) % max_samples_;
        samples_[idx].store(latency_ns, std::memory_order_relaxed);

        // 2. Increment Histogram Bucket (Approximate stats)
        // Divide by 10 because our resolution is 10ns per bucket
        size_t bucket_idx = std::min(latency_ns / 10, (uint64_t)1000);
        buckets_[bucket_idx].fetch_add(1, std::memory_order_relaxed);
        
        total_count_.fetch_add(1, std::memory_order_relaxed);
    }

    LatencyStats get_stats() const;
    void export_to_csv(const std::string& filename) const;

private:
    const size_t max_samples_;
    std::vector<std::atomic<uint64_t>> samples_; // Raw samples
    std::atomic<size_t> index_{0};
    
   
    std::vector<std::atomic<uint64_t>> buckets_; 
    std::atomic<uint64_t> total_count_{0};
};