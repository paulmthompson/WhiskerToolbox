#ifndef ANALOG_EVENT_THRESHOLD_VECTORS_HPP
#define ANALOG_EVENT_THRESHOLD_VECTORS_HPP

#include <string_view>
#include <vector>

/**
 * @file analog_event_threshold_vectors.hpp
 * @brief Shared input/output test vectors for AnalogEventThreshold (v1 and v2)
 */

namespace analog_event_threshold_vectors {

enum class Direction { positive, negative, absolute };

/**
 * @brief Single algorithm test row: signal, parameters, and expected event timestamps
 */
struct Case {
    std::string_view name;
    std::string_view dm_key;
    std::vector<float> values;
    std::vector<int> times;
    float threshold;
    Direction direction;
    float lockout;
    std::vector<int> expected_event_times;
};

inline std::vector<Case> const& algorithmCases() {
    static std::vector<Case> const cases = {
            {.name = "positive_no_lockout",
             .dm_key = "positive_no_lockout",
             .values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f},
             .times = {100, 200, 300, 400, 500},
             .threshold = 1.0f,
             .direction = Direction::positive,
             .lockout = 0.0f,
             .expected_event_times = {200, 400, 500}},
            {.name = "positive_with_lockout",
             .dm_key = "positive_with_lockout",
             .values = {0.5f, 1.5f, 1.8f, 0.5f, 2.5f, 2.2f},
             .times = {100, 200, 300, 400, 500, 600},
             .threshold = 1.0f,
             .direction = Direction::positive,
             .lockout = 150.0f,
             .expected_event_times = {200, 500}},
            {.name = "negative_no_lockout",
             .dm_key = "negative_no_lockout",
             .values = {0.5f, -1.5f, -0.8f, -2.5f, -1.2f},
             .times = {100, 200, 300, 400, 500},
             .threshold = -1.0f,
             .direction = Direction::negative,
             .lockout = 0.0f,
             .expected_event_times = {200, 400, 500}},
            {.name = "negative_with_lockout",
             .dm_key = "negative_with_lockout",
             .values = {0.0f, -1.5f, -1.2f, 0.0f, -2.0f, -0.5f},
             .times = {100, 200, 300, 400, 500, 600},
             .threshold = -1.0f,
             .direction = Direction::negative,
             .lockout = 150.0f,
             .expected_event_times = {200, 500}},
            {.name = "absolute_no_lockout",
             .dm_key = "absolute_no_lockout",
             .values = {0.5f, -1.5f, 0.8f, 2.5f, -1.2f, 0.9f},
             .times = {100, 200, 300, 400, 500, 600},
             .threshold = 1.0f,
             .direction = Direction::absolute,
             .lockout = 0.0f,
             .expected_event_times = {200, 400, 500}},
            {.name = "absolute_with_lockout",
             .dm_key = "absolute_with_lockout",
             .values = {0.5f, 1.5f, -1.2f, 0.5f, -2.0f, 0.8f},
             .times = {100, 200, 300, 400, 500, 600},
             .threshold = 1.0f,
             .direction = Direction::absolute,
             .lockout = 150.0f,
             .expected_event_times = {200, 500}},
            {.name = "no_events_high_threshold",
             .dm_key = "no_events_high_threshold",
             .values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f},
             .times = {100, 200, 300, 400, 500},
             .threshold = 10.0f,
             .direction = Direction::positive,
             .lockout = 0.0f,
             .expected_event_times = {}},
            {.name = "all_events_low_threshold",
             .dm_key = "all_events_low_threshold",
             .values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f},
             .times = {100, 200, 300, 400, 500},
             .threshold = 0.1f,
             .direction = Direction::positive,
             .lockout = 0.0f,
             .expected_event_times = {100, 200, 300, 400, 500}},
            {.name = "empty_signal",
             .dm_key = "empty_signal",
             .values = {},
             .times = {},
             .threshold = 1.0f,
             .direction = Direction::positive,
             .lockout = 0.0f,
             .expected_event_times = {}},
            {.name = "lockout_larger_than_duration",
             .dm_key = "lockout_larger_than_duration",
             .values = {1.5f, 2.5f, 3.5f},
             .times = {100, 200, 300},
             .threshold = 1.0f,
             .direction = Direction::positive,
             .lockout = 1000.0f,
             .expected_event_times = {100}},
            {.name = "events_at_threshold_positive",
             .dm_key = "events_at_threshold",
             .values = {0.5f, 1.0f, 1.5f},
             .times = {100, 200, 300},
             .threshold = 1.0f,
             .direction = Direction::positive,
             .lockout = 0.0f,
             .expected_event_times = {300}},
            {.name = "zero_based_timestamps",
             .dm_key = "zero_based_timestamps",
             .values = {1.5f, 0.5f, 2.5f},
             .times = {0, 10, 20},
             .threshold = 1.0f,
             .direction = Direction::positive,
             .lockout = 5.0f,
             .expected_event_times = {0, 20}},
    };
    return cases;
}

/**
 * @brief Look up a test case by DataManager key
 * @pre dm_key matches a row in algorithmCases()
 * @return Pointer to case, or nullptr if not found
 */
inline Case const* findCaseByDmKey(std::string_view dm_key) {
    for (auto const& tc : algorithmCases()) {
        if (tc.dm_key == dm_key) {
            return &tc;
        }
    }
    return nullptr;
}

} // namespace analog_event_threshold_vectors

#endif // ANALOG_EVENT_THRESHOLD_VECTORS_HPP
