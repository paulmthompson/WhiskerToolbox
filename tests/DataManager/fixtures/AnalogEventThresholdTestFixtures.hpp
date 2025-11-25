#ifndef WHISKERTOOLBOX_ANALOG_EVENT_THRESHOLD_TEST_FIXTURES_HPP
#define WHISKERTOOLBOX_ANALOG_EVENT_THRESHOLD_TEST_FIXTURES_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp"

#include <memory>
#include <vector>

namespace WhiskerToolbox::Testing {

/**
 * @brief Base fixture for analog event threshold testing
 * 
 * Provides shared setup with AnalogTimeSeries and TimeFrame.
 * Derived fixtures create specific test scenarios.
 */
struct AnalogEventThresholdFixture {
    std::shared_ptr<AnalogTimeSeries> analog_time_series;
    std::shared_ptr<TimeFrame> time_frame;
    
    AnalogEventThresholdFixture() {
        time_frame = std::make_shared<TimeFrame>();
    }
};

/**
 * @brief Positive threshold with no lockout
 * Values: {0.5, 1.5, 0.8, 2.5, 1.2}, Times: {100, 200, 300, 400, 500}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 0.0
 * Expected events: {200, 400, 500} (values > 1.0)
 */
struct PositiveThresholdNoLockout : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)
    };
    
    PositiveThresholdNoLockout() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Positive threshold with lockout
 * Values: {0.5, 1.5, 1.8, 0.5, 2.5, 2.2}, Times: {100, 200, 300, 400, 500, 600}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 150.0
 * Expected events: {200, 500} (300 filtered by lockout from 200, 600 filtered by lockout from 500)
 */
struct PositiveThresholdWithLockout : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 1.8f, 0.5f, 2.5f, 2.2f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500), TimeFrameIndex(600)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(500)
    };
    
    PositiveThresholdWithLockout() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 150.0;
    }
};

/**
 * @brief Negative threshold with no lockout
 * Values: {0.5, -1.5, -0.8, -2.5, -1.2}, Times: {100, 200, 300, 400, 500}
 * Threshold: -1.0, Direction: NEGATIVE, Lockout: 0.0
 * Expected events: {200, 400, 500} (values < -1.0)
 */
struct NegativeThresholdNoLockout : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, -1.5f, -0.8f, -2.5f, -1.2f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)
    };
    
    NegativeThresholdNoLockout() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = -1.0;
        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Negative threshold with lockout
 * Values: {0.0, -1.5, -1.2, 0.0, -2.0, -0.5}, Times: {100, 200, 300, 400, 500, 600}
 * Threshold: -1.0, Direction: NEGATIVE, Lockout: 150.0
 * Expected events: {200, 500} (300 filtered by lockout from 200)
 */
struct NegativeThresholdWithLockout : AnalogEventThresholdFixture {
    std::vector<float> values = {0.0f, -1.5f, -1.2f, 0.0f, -2.0f, -0.5f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500), TimeFrameIndex(600)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(500)
    };
    
    NegativeThresholdWithLockout() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = -1.0;
        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 150.0;
    }
};

/**
 * @brief Absolute threshold with no lockout
 * Values: {0.5, -1.5, 0.8, 2.5, -1.2, 0.9}, Times: {100, 200, 300, 400, 500, 600}
 * Threshold: 1.0, Direction: ABSOLUTE, Lockout: 0.0
 * Expected events: {200, 400, 500} (|values| > 1.0)
 */
struct AbsoluteThresholdNoLockout : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, -1.5f, 0.8f, 2.5f, -1.2f, 0.9f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500), TimeFrameIndex(600)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)
    };
    
    AbsoluteThresholdNoLockout() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::ABSOLUTE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Absolute threshold with lockout
 * Values: {0.5, 1.5, -1.2, 0.5, -2.0, 0.8}, Times: {100, 200, 300, 400, 500, 600}
 * Threshold: 1.0, Direction: ABSOLUTE, Lockout: 150.0
 * Expected events: {200, 500} (300 filtered by lockout from 200)
 */
struct AbsoluteThresholdWithLockout : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, -1.2f, 0.5f, -2.0f, 0.8f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500), TimeFrameIndex(600)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(500)
    };
    
    AbsoluteThresholdWithLockout() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::ABSOLUTE;
        params.lockoutTime = 150.0;
    }
};

/**
 * @brief Threshold too high - no events expected
 * Values: {0.5, 1.5, 0.8, 2.5, 1.2}, Times: {100, 200, 300, 400, 500}
 * Threshold: 10.0, Direction: POSITIVE, Lockout: 0.0
 * Expected events: {} (none exceed threshold)
 */
struct ThresholdTooHigh : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {};
    static constexpr size_t expected_num_results = 0;
    
    ThresholdTooHigh() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 10.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Threshold very low - all events expected
 * Values: {0.5, 1.5, 0.8, 2.5, 1.2}, Times: {100, 200, 300, 400, 500}
 * Threshold: 0.1, Direction: POSITIVE, Lockout: 0.0
 * Expected events: {100, 200, 300, 400, 500} (all exceed threshold)
 */
struct ThresholdVeryLow : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500)
    };
    
    ThresholdVeryLow() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 0.1;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Empty AnalogTimeSeries (no timestamps/values)
 * Expected events: {} (empty)
 */
struct EmptyAnalogTimeSeries : AnalogEventThresholdFixture {
    std::vector<float> values = {};
    std::vector<TimeFrameIndex> times = {};
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {};
    static constexpr size_t expected_num_results = 0;
    
    EmptyAnalogTimeSeries() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Lockout time larger than series duration
 * Values: {1.5, 2.5, 3.5}, Times: {100, 200, 300}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 500.0
 * Expected events: {100} (only first event, others filtered by lockout)
 */
struct LargeLockoutTime : AnalogEventThresholdFixture {
    std::vector<float> values = {1.5f, 2.5f, 3.5f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(100)};
    
    LargeLockoutTime() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 500.0;
    }
};

/**
 * @brief Events exactly at threshold value (positive direction)
 * Values: {0.5, 1.0, 1.5}, Times: {100, 200, 300}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 0.0
 * Expected events: {300} (1.0 is not > 1.0, so only 1.5 counts)
 */
struct EventsAtThresholdPositive : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.0f, 1.5f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(300)};
    
    EventsAtThresholdPositive() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Events exactly at threshold value (negative direction)
 * Values: {0.5, 1.0, 1.5}, Times: {100, 200, 300}
 * Threshold: 0.5, Direction: NEGATIVE, Lockout: 0.0
 * Expected events: {} (0.5 is not < 0.5)
 */
struct EventsAtThresholdNegative : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.0f, 1.5f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {};
    static constexpr size_t expected_num_results = 0;
    
    EventsAtThresholdNegative() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 0.5;
        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Timestamps starting from zero
 * Values: {1.5, 0.5, 2.5}, Times: {0, 10, 20}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 5.0
 * Expected events: {0, 20} (10 is below threshold)
 */
struct TimestampsFromZero : AnalogEventThresholdFixture {
    std::vector<float> values = {1.5f, 0.5f, 2.5f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(0), TimeFrameIndex(20)
    };
    
    TimestampsFromZero() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 5.0;
    }
};

/**
 * @brief Progress callback test fixture
 * Values: {0.5, 1.5, 0.8, 2.5, 1.2}, Times: {100, 200, 300, 400, 500}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 0.0
 * Expected progress sequence: {20, 40, 60, 80, 100, 100}
 */
struct ProgressCallbackTest : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)
    };
    std::vector<int> expected_progress_sequence = {20, 40, 60, 80, 100, 100};
    static constexpr size_t expected_num_samples = 5;
    
    ProgressCallbackTest() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief JSON pipeline test - positive threshold
 * Values: {0.5, 1.5, 0.8, 2.5, 1.2}, Times: {100, 200, 300, 400, 500}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 0.0
 * Expected events: {200, 400, 500}
 */
struct JsonPipelinePositiveThreshold : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)
    };
    
    JsonPipelinePositiveThreshold() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        analog_time_series->setTimeFrame(time_frame);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief JSON pipeline test - with lockout
 * Values: {0.5, 1.5, 0.8, 2.5, 1.2, 0.3}, Times: {100, 200, 300, 400, 500, 600}
 * Threshold: 1.0, Direction: POSITIVE, Lockout: 150.0
 * Expected events: {200, 400} (500 filtered by lockout from 400)
 */
struct JsonPipelineWithLockout : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f, 0.3f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500), TimeFrameIndex(600)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(400)
    };
    
    JsonPipelineWithLockout() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        analog_time_series->setTimeFrame(time_frame);
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 150.0;
    }
};

/**
 * @brief JSON pipeline test - absolute threshold
 * Values: {0.5, 1.5, 0.8, 2.5, 1.2, 0.3}, Times: {100, 200, 300, 400, 500, 600}
 * Threshold: 1.3, Direction: ABSOLUTE, Lockout: 0.0
 * Expected events: {200, 400} (only |1.5| > 1.3 and |2.5| > 1.3)
 */
struct JsonPipelineAbsoluteThreshold : AnalogEventThresholdFixture {
    std::vector<float> values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f, 0.3f};
    std::vector<TimeFrameIndex> times = {
        TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
        TimeFrameIndex(400), TimeFrameIndex(500), TimeFrameIndex(600)
    };
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events = {
        TimeFrameIndex(200), TimeFrameIndex(400)
    };
    
    JsonPipelineAbsoluteThreshold() {
        analog_time_series = std::make_shared<AnalogTimeSeries>(values, times);
        analog_time_series->setTimeFrame(time_frame);
        params.thresholdValue = 1.3;
        params.direction = ThresholdParams::ThresholdDirection::ABSOLUTE;
        params.lockoutTime = 0.0;
    }
};

/**
 * @brief Parameter factory test fixture
 * Used to test JSON parameter parsing for ThresholdParams
 * 
 * This fixture provides expected values for testing the ParameterFactory's ability
 * to parse JSON configuration into ThresholdParams. The expected values should match
 * the JSON input used in the test:
 * {
 *     "threshold_value": 2.5,
 *     "direction": "Negative (Falling)",
 *     "lockout_time": 123.45
 * }
 * 
 * No analog time series is needed for parameter factory testing.
 */
struct ParameterFactoryTest : AnalogEventThresholdFixture {
    static constexpr double expected_threshold_value = 2.5;
    static constexpr double expected_lockout_time = 123.45;
    static constexpr ThresholdParams::ThresholdDirection expected_direction = 
        ThresholdParams::ThresholdDirection::NEGATIVE;
    
    ParameterFactoryTest() = default;
};

} // namespace WhiskerToolbox::Testing

#endif // WHISKERTOOLBOX_ANALOG_EVENT_THRESHOLD_TEST_FIXTURES_HPP
