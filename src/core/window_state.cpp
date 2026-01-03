#include <core/window_state.h>
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
    return (t_ms / w_ms) * w_ms;
}