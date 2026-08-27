/**
 * @file ScatterPlotStressFixture.hpp
 * @brief Synthetic DataManager setup for ScatterPlot view interaction stress runs.
 */

#ifndef SCATTER_PLOT_STRESS_FIXTURE_HPP
#define SCATTER_PLOT_STRESS_FIXTURE_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Core/ScatterAxisSource.hpp"
#include "Core/ScatterPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Neuralyzer::Benchmark {

constexpr char const * kScatterStressTimeKey = "time";
constexpr char const * kScatterStressXKey = "scatter_x";
constexpr char const * kScatterStressYKey = "scatter_y";

/**
 * @brief Generate deterministic analog values for scatter stress inputs.
 * @pre count may be zero.
 * @post Returns count finite values.
 */
[[nodiscard]] inline std::vector<float> generateScatterStressValues(std::size_t count, double phase) {
    std::vector<float> values;
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto const t = static_cast<double>(i) * 0.001;
        values.push_back(static_cast<float>(std::sin(t + phase)));
    }
    return values;
}

/**
 * @brief Create a dense TimeFrame for synthetic scatter data.
 * @pre count must fit in the int values accepted by TimeFrame.
 * @post The returned TimeFrame contains indices [0, count).
 */
[[nodiscard]] inline std::shared_ptr<TimeFrame> makeScatterStressTimeFrame(std::size_t count) {
    std::vector<int> times(count);
    for (std::size_t i = 0; i < count; ++i) {
        times[i] = static_cast<int>(i);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Add an AnalogTimeSeries to a DataManager under the stress time key.
 * @pre dm must already contain the stress TimeFrame.
 * @post The series is available under key.
 */
inline void addScatterStressAnalogSeries(
        DataManager & dm,
        std::string const & key,
        std::vector<float> values) {
    auto const count = values.size();
    auto series = std::make_shared<AnalogTimeSeries>(std::move(values), count);
    series->setTimeFrame(dm.getTime(TimeKey(kScatterStressTimeKey)));
    dm.setData<AnalogTimeSeries>(key, series, TimeKey(kScatterStressTimeKey));
}

/**
 * @brief Build a DataManager with two analog series for scatter plotting.
 * @pre point_count may be zero.
 * @post The returned manager contains X and Y analog series.
 */
[[nodiscard]] inline std::shared_ptr<DataManager> makeScatterStressDataManager(std::size_t point_count) {
    auto dm = std::make_shared<DataManager>();
    dm->setTime(TimeKey(kScatterStressTimeKey), makeScatterStressTimeFrame(point_count), true);
    addScatterStressAnalogSeries(
            *dm, kScatterStressXKey, generateScatterStressValues(point_count, 0.0));
    addScatterStressAnalogSeries(
            *dm, kScatterStressYKey, generateScatterStressValues(point_count, 1.0));
    return dm;
}

/**
 * @brief Configure scatter axis sources on state for the stress data keys.
 * @pre state must not be null.
 * @post X and Y sources reference the stress analog keys.
 */
inline void configureScatterStressSources(ScatterPlotState & state) {
    state.setXSource(ScatterAxisSource{.data_key = kScatterStressXKey});
    state.setYSource(ScatterAxisSource{.data_key = kScatterStressYKey});
}

/**
 * @brief Apply a deterministic pan/zoom update for stress iteration i.
 * @pre state must not be null.
 * @post ScatterPlotState has a new view transform.
 */
inline void applyScatterStressPanZoom(ScatterPlotState & state, std::size_t iteration) {
    auto const pan = static_cast<double>(iteration % 128U) * 0.001;
    auto const zoom = 1.0 + static_cast<double>(iteration % 16U) * 0.01;
    state.setPan(pan, -pan);
    state.setXZoom(zoom);
    state.setYZoom(zoom);
}

}// namespace Neuralyzer::Benchmark

#endif// SCATTER_PLOT_STRESS_FIXTURE_HPP
