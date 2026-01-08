#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace streamfeat{

    using FeatureValue = std::pair<int32_t, double>;

    struct Event{
        std::string entity_id;
        int64_t timestamp_ms;
        std::vector<FeatureValue> features;
    };

    struct EventBatch{
        int64_t enqueue_ms;
        std::vector<Event> events;
    };
}; //namespace streamfeat