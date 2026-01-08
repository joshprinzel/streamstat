#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory_resource>
#include <unordered_map>
#include <vector>

#include <fastnum/running_stats.hpp>

// WindowState
//
// Rolling window of per-feature online statistics over fixed-width time buckets.
// Designed for single-writer shard ownership (no internal locking).
//
// Core invariants:
// - Bucket width is fixed: W = bucket_width_ms_
// - Timestamp t maps to aligned bucket start: s = floor(t / W) * W
// - Window storage is fixed: N = num_buckets_ bucket slots (ring buffer)
// - Slot index: idx = (s / W) % N
// - Bucket slot is valid for s iff bucket.start_ms == s
// - On slot reuse (stale bucket), bucket is reset (clears per-feature stats)
//
// Query semantics:
// - Aggregate(feature_id, now_ms) returns a snapshot over buckets with start_ms in:
//   [Align(now_ms) - (N - 1) * W, Align(now_ms)]
// - Aggregate does not mutate window state.
//
// Complexity:
// - Observe: expected O(1) per (feature_id, value) update (hash-map access)
// - Aggregate: O(N) buckets + hash lookups
class WindowState {
public:
  explicit WindowState(int64_t bucket_width_ms,
                       std::size_t num_buckets,
                       std::pmr::memory_resource& mr);

  void Observe(int32_t feature_id, double value, int64_t timestamp_ms);

  [[nodiscard]] fastnum::RunningStats<double>
  Aggregate(int32_t feature_id, int64_t now_ms) const;

  int64_t bucket_width_ms() const noexcept { return bucket_width_ms_; }
  std::size_t num_buckets() const noexcept { return num_buckets_; }

private:
  struct Bucket {
    static constexpr int64_t kNeverUsed = std::numeric_limits<int64_t>::min();

    int64_t start_ms = kNeverUsed;
    std::pmr::unordered_map<int32_t, fastnum::RunningStats<double>> stats;

    explicit Bucket(std::pmr::memory_resource* mr) : stats(mr) {}

    void Reset(int64_t new_start_ms) {
      start_ms = new_start_ms;
      stats.clear();
      // Optional memory-bounding behavior:
      // stats.rehash(0);
    }
  };

  static constexpr int64_t Align_(int64_t t_ms, int64_t w_ms) noexcept {
    // Preconditions enforced at API boundary and/or via asserts in callers.
    return (t_ms / w_ms) * w_ms;
  }

  std::size_t RingIndexForAlignedStart_(int64_t aligned_start_ms) const noexcept;

  Bucket& GetOrResetBucketFor_(int64_t timestamp_ms);

  int64_t QueryStart_(int64_t now_ms) const noexcept;
  int64_t QueryEnd_(int64_t now_ms) const noexcept;

  int64_t bucket_width_ms_;
  std::size_t num_buckets_;
  std::pmr::vector<Bucket> buckets_;
};
