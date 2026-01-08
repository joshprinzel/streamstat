#pragma once
#include "streamfeat/core/window_state.h"
#include <algorithm>
// WindowState invariants:
//
// Time & alignment:
// - Bucket width W > 0; timestamps t >= 0.
// - Aligned slice start s = floor(t / W) * W.
//
// Storage & identity:
// - Fixed ring buffer with N slots (buckets_.size() == N).
// - A bucket is valid for slice s iff bucket.start_ms == s.
// - bucket.start_ms == Bucket::kNeverUsed means empty / ignored.
// - For any valid bucket: start_ms >= 0 and start_ms % W == 0.
//
// Ring placement:
// - Slot index for slice s is idx = (s / W) % N.
// - At most one bucket in the window may have start_ms == s.
//
// Mutation:
// - Observe(feature_id, value, ts) updates exactly one bucket
//   corresponding to Align_(ts, W).
// - On slot reuse (stale bucket), Reset(s) clears all previous stats.
// - Reset is total: no data from previous slices survives.
//
// Query:
// - For now_ms, query range is:
//     end   = Align_(now_ms, W)
//     start = end - (N - 1) * W
// - Aggregate includes buckets with start_ms ∈ [start, end].
// - Aggregate does not mutate state.
//
// Concurrency:
// - Single-writer shard ownership; no internal locking required.

class EntityState{
    public:
        static constexpr int64_t kBucketWidthMs = 5000;
        static constexpr std::size_t kShortBuckets = 60;
        static constexpr std::size_t kLongBuckets = 720;

        explicit EntityState(std::pmr::memory_resource& mr) 
            : last_seen_ms_(0),
            short_w_(kBucketWidthMs, kShortBuckets, mr),
            long_w_(kBucketWidthMs, kLongBuckets, mr) {}
        
        //Hot-path update. Caller (shard) should enforce drop/retention policy            
        inline void Observe(int32_t feature_id, double value, int64_t timestamp_ms){
            short_w_.Observe(feature_id, value, timestamp_ms);
            long_w_.Observe(feature_id, value, timestamp_ms);

            //Take the most recent time for last_seen_ms_
            last_seen_ms_ = std::max(last_seen_ms_, timestamp_ms);
        }

        [[nodiscard]] int64_t last_seen_ms() const noexcept{ return last_seen_ms_;}
        [[nodiscard]] const WindowState& short_window() const noexcept{ return short_w_;}
        [[nodiscard]] const WindowState& long_window() const noexcept{ return long_w_;}
        
    private:
        int64_t last_seen_ms_;
        WindowState short_w_; //5 min ring
        WindowState long_w_; //1 hr ring
};