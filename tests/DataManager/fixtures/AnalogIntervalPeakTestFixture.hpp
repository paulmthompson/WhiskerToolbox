#ifndef ANALOG_INTERVAL_PEAK_TEST_FIXTURE_HPP
#define ANALOG_INTERVAL_PEAK_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include <memory>
#include <vector>
#include <string>
#include <map>

class AnalogIntervalPeakTestFixture {
protected:
    AnalogIntervalPeakTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        populateTestData();
    }

    ~AnalogIntervalPeakTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    std::map<std::string, std::shared_ptr<AnalogTimeSeries>> m_test_analog_signals;
    std::map<std::string, std::shared_ptr<DigitalIntervalSeries>> m_test_interval_series;
    std::map<std::string, std::shared_ptr<TimeFrame>> m_test_timeframes;

private:
    void populateTestData() {
        // Data from "Basic maximum detection within intervals"
        createAnalogSignal("basic_max_within", {1.0f, 2.0f, 5.0f, 3.0f, 1.0f, 0.5f}, {0, 100, 200, 300, 400, 500});
        createIntervalSeries("basic_max_within_intervals", {{0, 200}, {300, 500}});

        // Data from "Maximum detection with progress callback"
        createAnalogSignal("max_with_progress", {1.0f, 5.0f, 2.0f, 8.0f, 3.0f}, {0, 10, 20, 30, 40});
        createIntervalSeries("max_with_progress_intervals", {{0, 20}, {30, 40}});

        // Data from "Multiple intervals with varying peak locations"
        createAnalogSignal("multiple_intervals_varying", {1.0f, 9.0f, 3.0f, 2.0f, 8.0f, 1.0f, 5.0f, 10.0f, 2.0f}, {0, 10, 20, 30, 40, 50, 60, 70, 80});
        createIntervalSeries("multiple_intervals_varying_intervals", {{0, 20}, {30, 50}, {60, 80}});

        // Data from "Basic minimum detection within intervals"
        createAnalogSignal("basic_min_within", {5.0f, 3.0f, 1.0f, 4.0f, 2.0f, 3.0f}, {0, 100, 200, 300, 400, 500});
        createIntervalSeries("basic_min_within_intervals", {{0, 200}, {300, 500}});

        // Data from "Minimum with negative values"
        createAnalogSignal("min_with_negative", {1.0f, -5.0f, 2.0f, -3.0f, 0.5f}, {0, 10, 20, 30, 40});
        createIntervalSeries("min_with_negative_intervals", {{0, 20}, {20, 40}});

        // Data from "Maximum between interval starts"
        createAnalogSignal("max_between_starts", {1.0f, 2.0f, 5.0f, 8.0f, 10.0f, 7.0f, 3.0f}, {0, 10, 20, 30, 40, 50, 60});
        createIntervalSeries("max_between_starts_intervals", {{0, 10}, {20, 30}, {40, 50}});

        // Data from "Minimum between interval starts"
        createAnalogSignal("min_between_starts", {5.0f, 2.0f, 8.0f, 3.0f, 9.0f, 1.0f}, {0, 100, 200, 300, 400, 500});
        createIntervalSeries("min_between_starts_intervals", {{0, 100}, {200, 300}, {400, 500}});

        // Data from "Empty intervals - no events"
        createAnalogSignal("empty_intervals", {1.0f, 2.0f, 3.0f}, {0, 10, 20});
        createIntervalSeries("empty_intervals_intervals", {});

        // Data from "Interval with no corresponding analog data"
        createAnalogSignal("no_data_interval", {1.0f, 2.0f, 3.0f}, {0, 10, 20});
        createIntervalSeries("no_data_interval_intervals", {{100, 200}});

        // Data from "Single data point interval"
        createAnalogSignal("single_point", {1.0f, 5.0f, 2.0f}, {0, 10, 20});
        createIntervalSeries("single_point_intervals", {{10, 10}});

        // Data from "Multiple intervals, some without data"
        createAnalogSignal("mixed_data_availability", {1.0f, 5.0f, 8.0f}, {0, 10, 20});
        createIntervalSeries("mixed_data_availability_intervals", {{0, 10}, {50, 60}, {10, 20}});

        // Data from "Different timeframes - conversion required"
        createAnalogSignalWithTimeFrame("different_timeframes", {1.0f, 5.0f, 2.0f, 8.0f, 3.0f}, {0, 1, 2, 3, 4}, {0, 10, 20, 30, 40});
        createIntervalSeriesWithTimeFrame("different_timeframes_intervals", {{1, 3}}, {0, 5, 15, 25, 35});

        // Data from "Same timeframe - no conversion needed"
        createAnalogSignalWithTimeFrame("same_timeframe", {1.0f, 9.0f, 3.0f, 5.0f}, {0, 1, 2, 3}, {0, 10, 20, 30});
        createIntervalSeriesWithTimeFrame("same_timeframe_intervals", {{0, 2}}, {0, 10, 20, 30});

        // Data for operation interface tests
        createAnalogSignal("operation_interface", {1.0f, 5.0f, 2.0f, 8.0f, 3.0f}, {0, 10, 20, 30, 40});
        createIntervalSeries("operation_interface_intervals", {{0, 20}, {30, 40}});

        // Data for operation with progress callback
        createAnalogSignal("operation_progress", {1.0f, 5.0f, 2.0f, 8.0f, 3.0f}, {0, 10, 20, 30, 40});
        createIntervalSeries("operation_progress_intervals", {{0, 20}});

        // Additional signals for edge case testing
        createAnalogSignal("simple_signal", {1.0f, 2.0f, 3.0f}, {0, 10, 20});
    }

    void createAnalogSignal(const std::string& key, const std::vector<float>& values, const std::vector<int>& times_int) {
        std::vector<TimeFrameIndex> times;
        for (int t : times_int) {
            times.push_back(TimeFrameIndex(t));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        auto time_frame = std::make_shared<TimeFrame>(times_int);
        ats->setTimeFrame(time_frame);
        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
    }

    void createAnalogSignalWithTimeFrame(const std::string& key, const std::vector<float>& values, 
                                         const std::vector<int>& times_int, const std::vector<int>& timeframe_values) {
        std::vector<TimeFrameIndex> times;
        for (int t : times_int) {
            times.push_back(TimeFrameIndex(t));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);
        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createIntervalSeries(const std::string& key, const std::vector<Interval>& intervals) {
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        m_data_manager->setData(key, dis, TimeKey(key + "_time"));
        m_test_interval_series[key] = dis;
    }

    void createIntervalSeriesWithTimeFrame(const std::string& key, const std::vector<Interval>& intervals,
                                           const std::vector<int>& timeframe_values) {
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        dis->setTimeFrame(time_frame);
        m_data_manager->setData(key, dis, TimeKey(key + "_time"));
        m_test_interval_series[key] = dis;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    std::unique_ptr<DataManager> m_data_manager;
};

#endif // ANALOG_INTERVAL_PEAK_TEST_FIXTURE_HPP
