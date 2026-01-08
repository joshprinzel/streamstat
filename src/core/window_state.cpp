#include "streamfeat/core/window_state.h"

#include <cassert>
#include <utility>

WindowState::WindowState(int64_t bucket_width_ms,
                         std::size_t num_buckets,
                         std::pmr::memory_resource& mr)
  : bucket_width_ms_(bucket_width_ms),
    num_buckets_(num_buckets),
    buckets_(&mr)
{
  // Guardrails before usage
  assert(bucket_width_ms_ > 0);
  assert(num_buckets_ > 0);

  buckets_.reserve(num_buckets_);
  for (std::size_t i = 0; i < num_buckets_; ++i) {
    buckets_.emplace_back(&mr);
  }
}

std::size_t WindowState::RingIndexForAlignedStart_(int64_t aligned_start_ms) const noexcept {
  // aligned_start_ms assumed >= 0 by API contract; if you relax that,
  // implement a positive-mod helper.
  const int64_t bucket_number = aligned_start_ms / bucket_width_ms_;
  const std::size_t idx = static_cast<std::size_t>(
      bucket_number % static_cast<int64_t>(num_buckets_));
  return idx;
}

WindowState::Bucket& WindowState::GetOrResetBucketFor_(int64_t timestamp_ms) {
  const int64_t aligned = Align_(timestamp_ms, bucket_width_ms_);
  const std::size_t idx = RingIndexForAlignedStart_(aligned);

  Bucket& b = buckets_[idx];
  if (b.start_ms != aligned) {
    b.Reset(aligned);
  }
  return b;
}

void WindowState::Observe(int32_t feature_id, double value, int64_t timestamp_ms) {
  // Public API contract checks (debug)
  assert(timestamp_ms >= 0);
  // If negative feature IDs are invalid in your system, enforce it:
  // assert(feature_id >= 0);

  Bucket& b = GetOrResetBucketFor_(timestamp_ms);
  b.stats[feature_id].observe(value);
}

int64_t WindowState::QueryEnd_(int64_t now_ms) const noexcept {
  return Align_(now_ms, bucket_width_ms_);
}

int64_t WindowState::QueryStart_(int64_t now_ms) const noexcept {
  // Inclusive start of oldest bucket to include
  return QueryEnd_(now_ms) - static_cast<int64_t>(num_buckets_ - 1) * bucket_width_ms_;
}

fastnum::RunningStats<double>
WindowState::Aggregate(int32_t feature_id, int64_t now_ms) const {
  // Public API contract checks (debug)
  assert(now_ms >= 0);
  // assert(feature_id >= 0);

  const int64_t start = QueryStart_(now_ms);
  const int64_t end = QueryEnd_(now_ms);

  fastnum::RunningStats<double> output;
  for (const Bucket& b : buckets_) {
    if (b.start_ms == Bucket::kNeverUsed) continue;
    if (b.start_ms < start || b.start_ms > end) continue;

    const auto it = b.stats.find(feature_id);
    if (it != b.stats.end()) {
      output.merge(it->second);
    }
  }
  return output;
}
