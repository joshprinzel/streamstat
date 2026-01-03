#include "core/window_state.h"
#include <cassert>


WindowState::WindowState(int64_t bucket_width_ms, int32_t num_buckets, std::pmr::memory_resource* mr)
    : bucket_width_ms_(bucket_width_ms),
      num_buckets_(num_buckets),
      buckets_(mr) {

    //Guardrails before construction
    assert(mr != nullptr);
    assert(bucket_width_ms_ > 0);
    assert(num_buckets_ > 0);

    //Reserve enough memory in the vector for the number of buckets
    buckets_.reserve(static_cast<size_t>(num_buckets_));
    for(int32_t i = 0; i < num_buckets_ ; ++i){
        /* emplace_back() def:
            function constructs the element in-place, directly within the vector's
            managed memory. It forwards its arguments to the element's constructor*/
        buckets_.emplace_back(mr);
    }
}

int64_t WindowState::Align_(int64_t t_ms, int64_t w_ms){
    //Assumes w_ms > 0. Integer division floor toward zero; timestamps should be >= 0 
    assert(w_ms > 0);
    assert(t_ms >= 0);
    return (t_ms / w_ms) * w_ms;
}

int32_t WindowState::IndexFromAlignedStart_(int64_t aligned_start_ms) const{
    const int64_t bucket_number = aligned_start_ms / bucket_width_ms_;
    const int64_t idx = bucket_number % num_buckets_;

    return static_cast<int32_t>(idx);
}

WindowState::Bucket& WindowState::BucketFor_(int64_t timestamp_ms){
    const int64_t aligned = Align_(timestamp_ms, bucket_width_ms_);
    const int32_t idx = IndexFromAlignedStart_(aligned);

    Bucket& b = buckets_[static_cast<size_t>(idx)];
    if(b.start_ms != aligned){
        b.Reset(aligned);
    }
    return b;
}

void WindowState::Observe(int32_t feature_id, double value, int64_t timestamp_ms){
    Bucket& b = BucketFor_(timestamp_ms);
    b.stats[feature_id].observe(value); //feature_id key in map; observe() from fastnum API
}

int64_t WindowState::QueryEnd_(int64_t now_ms) const{
    return Align_(now_ms, bucket_width_ms_);
}

int64_t WindowState::QueryStart_(int64_t now_ms) const{
    //Inclusive start of aldest bucket to include
    return QueryEnd_(now_ms) - (static_cast<int64_t>(num_buckets_) - 1) * bucket_width_ms_;
}

fastnum::RunningStats<double> WindowState::Aggregate(int32_t feature_id, int64_t now_ms) const{
    const int64_t start = QueryStart_(now_ms);
    const int64_t end = QueryEnd_(now_ms);

    fastnum::RunningStats<double> output;
    for(const Bucket& b : buckets_){
        if(b.start_ms == Bucket::kNeverUsed) continue;
        if(b.start_ms < start || b.start_ms > end) continue;

        auto it = b.stats.find(feature_id);
        if(it != b.stats.end()){
            output.merge(it->second);
        }
    }
    return output;
}