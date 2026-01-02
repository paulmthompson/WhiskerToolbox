#include "TimeSeriesDataStore.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

namespace DataViewer {

TimeSeriesDataStore::TimeSeriesDataStore(QObject * parent)
    : QObject(parent) {}

// ============================================================================
// Add Series Methods
// ============================================================================

void TimeSeriesDataStore::addAnalogSeries(std::string const & key,
                                          std::shared_ptr<AnalogTimeSeries> series,
                                          std::string const & color) {
    auto display_options = std::make_unique<NewAnalogTimeSeriesDisplayOptions>();

    // Set color
    display_options->style.hex_color = color.empty()
                                               ? DefaultColors::getColorForIndex(_analog_series.size())
                                               : color;
    display_options->style.is_visible = true;

    // Calculate scale factor based on standard deviation
    auto start_time = std::chrono::high_resolution_clock::now();
    setAnalogIntrinsicProperties(series.get(), *display_options);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Standard deviation calculation took " << duration.count() << " milliseconds" << std::endl;

    display_options->scale_factor = display_options->data_cache.cached_std_dev * 5.0f;
    display_options->user_scale_factor = 1.0f;// Default user scale

    // Configure gap detection based on data density
    if (series->getTimeFrame()->getTotalFrameCount() / 5 > series->getNumSamples()) {
        display_options->gap_handling = AnalogGapHandling::AlwaysConnect;
        display_options->enable_gap_detection = false;
    } else {
        display_options->enable_gap_detection = true;
        display_options->gap_handling = AnalogGapHandling::DetectGaps;
        // Set gap threshold to 0.1% of total frames, with a minimum floor of 2
        float const calculated_threshold = static_cast<float>(series->getTimeFrame()->getTotalFrameCount()) / 1000.0f;
        display_options->gap_threshold = std::max(2.0f, calculated_threshold);
    }

    _analog_series[key] = AnalogSeriesEntry{
            std::move(series),
            std::move(display_options)};

    emit seriesAdded(QString::fromStdString(key), QStringLiteral("analog"));
    emit layoutDirty();
}

void TimeSeriesDataStore::addEventSeries(std::string const & key,
                                         std::shared_ptr<DigitalEventSeries> series,
                                         std::string const & color) {
    auto display_options = std::make_unique<NewDigitalEventSeriesDisplayOptions>();

    // Set color
    display_options->style.hex_color = color.empty()
                                               ? DefaultColors::getColorForIndex(_digital_event_series.size())
                                               : color;
    display_options->style.is_visible = true;

    _digital_event_series[key] = DigitalEventSeriesEntry{
            std::move(series),
            std::move(display_options)};

    emit seriesAdded(QString::fromStdString(key), QStringLiteral("event"));
    emit layoutDirty();
}

void TimeSeriesDataStore::addIntervalSeries(std::string const & key,
                                            std::shared_ptr<DigitalIntervalSeries> series,
                                            std::string const & color) {
    auto display_options = std::make_unique<NewDigitalIntervalSeriesDisplayOptions>();

    // Set color
    display_options->style.hex_color = color.empty()
                                               ? DefaultColors::getColorForIndex(_digital_interval_series.size())
                                               : color;
    display_options->style.is_visible = true;

    _digital_interval_series[key] = DigitalIntervalSeriesEntry{
            std::move(series),
            std::move(display_options)};

    emit seriesAdded(QString::fromStdString(key), QStringLiteral("interval"));
    emit layoutDirty();
}

// ============================================================================
// Remove Series Methods
// ============================================================================

bool TimeSeriesDataStore::removeAnalogSeries(std::string const & key) {
    auto it = _analog_series.find(key);
    if (it != _analog_series.end()) {
        _analog_series.erase(it);
        emit seriesRemoved(QString::fromStdString(key));
        emit layoutDirty();
        return true;
    }
    return false;
}

bool TimeSeriesDataStore::removeEventSeries(std::string const & key) {
    auto it = _digital_event_series.find(key);
    if (it != _digital_event_series.end()) {
        _digital_event_series.erase(it);
        emit seriesRemoved(QString::fromStdString(key));
        emit layoutDirty();
        return true;
    }
    return false;
}

bool TimeSeriesDataStore::removeIntervalSeries(std::string const & key) {
    auto it = _digital_interval_series.find(key);
    if (it != _digital_interval_series.end()) {
        _digital_interval_series.erase(it);
        emit seriesRemoved(QString::fromStdString(key));
        emit layoutDirty();
        return true;
    }
    return false;
}

void TimeSeriesDataStore::clearAll() {
    if (isEmpty()) {
        return;
    }

    // Collect keys before clearing for signal emission
    std::vector<std::string> analog_keys;
    std::vector<std::string> event_keys;
    std::vector<std::string> interval_keys;

    analog_keys.reserve(_analog_series.size());
    event_keys.reserve(_digital_event_series.size());
    interval_keys.reserve(_digital_interval_series.size());

    for (auto const & [key, _]: _analog_series) {
        analog_keys.push_back(key);
    }
    for (auto const & [key, _]: _digital_event_series) {
        event_keys.push_back(key);
    }
    for (auto const & [key, _]: _digital_interval_series) {
        interval_keys.push_back(key);
    }

    // Clear all maps
    _analog_series.clear();
    _digital_event_series.clear();
    _digital_interval_series.clear();

    // Emit cleared signal first
    emit cleared();

    // Emit individual seriesRemoved signals
    for (auto const & key: analog_keys) {
        emit seriesRemoved(QString::fromStdString(key));
    }
    for (auto const & key: event_keys) {
        emit seriesRemoved(QString::fromStdString(key));
    }
    for (auto const & key: interval_keys) {
        emit seriesRemoved(QString::fromStdString(key));
    }

    emit layoutDirty();
}

// ============================================================================
// Series Accessors
// ============================================================================

std::unordered_map<std::string, AnalogSeriesEntry> const & TimeSeriesDataStore::analogSeries() const {
    return _analog_series;
}

std::unordered_map<std::string, DigitalEventSeriesEntry> const & TimeSeriesDataStore::eventSeries() const {
    return _digital_event_series;
}

std::unordered_map<std::string, DigitalIntervalSeriesEntry> const & TimeSeriesDataStore::intervalSeries() const {
    return _digital_interval_series;
}

std::unordered_map<std::string, AnalogSeriesEntry> & TimeSeriesDataStore::analogSeriesMutable() {
    return _analog_series;
}

std::unordered_map<std::string, DigitalEventSeriesEntry> & TimeSeriesDataStore::eventSeriesMutable() {
    return _digital_event_series;
}

std::unordered_map<std::string, DigitalIntervalSeriesEntry> & TimeSeriesDataStore::intervalSeriesMutable() {
    return _digital_interval_series;
}

// ============================================================================
// Display Options Accessors
// ============================================================================

std::optional<NewAnalogTimeSeriesDisplayOptions *> TimeSeriesDataStore::getAnalogConfig(std::string const & key) {
    auto it = _analog_series.find(key);
    if (it != _analog_series.end() && it->second.display_options) {
        return it->second.display_options.get();
    }
    return std::nullopt;
}

std::optional<NewDigitalEventSeriesDisplayOptions *> TimeSeriesDataStore::getEventConfig(std::string const & key) {
    auto it = _digital_event_series.find(key);
    if (it != _digital_event_series.end() && it->second.display_options) {
        return it->second.display_options.get();
    }
    return std::nullopt;
}

std::optional<NewDigitalIntervalSeriesDisplayOptions *> TimeSeriesDataStore::getIntervalConfig(std::string const & key) {
    auto it = _digital_interval_series.find(key);
    if (it != _digital_interval_series.end() && it->second.display_options) {
        return it->second.display_options.get();
    }
    return std::nullopt;
}

// ============================================================================
// Layout System Integration
// ============================================================================

CorePlotting::LayoutRequest TimeSeriesDataStore::buildLayoutRequest(
        float viewport_y_min,
        float viewport_y_max,
        SpikeSorterConfigMap const & spike_sorter_configs) const {

    CorePlotting::LayoutRequest request;
    request.viewport_y_min = viewport_y_min;
    request.viewport_y_max = viewport_y_max;

    // Collect visible analog series keys and order by spike sorter config
    std::vector<std::string> visible_analog_keys;
    for (auto const & [key, data]: _analog_series) {
        if (data.display_options->style.is_visible) {
            visible_analog_keys.push_back(key);
        }
    }

    // Apply spike sorter ordering if any configs exist
    if (!spike_sorter_configs.empty()) {
        visible_analog_keys = orderKeysBySpikeSorterConfig(visible_analog_keys, spike_sorter_configs);
    }

    // Add analog series in order
    for (auto const & key: visible_analog_keys) {
        request.series.emplace_back(key, CorePlotting::SeriesType::Analog, true);
    }

    // Add digital event series (stacked events after analog series, full-canvas events as non-stackable)
    for (auto const & [key, data]: _digital_event_series) {
        if (!data.display_options->style.is_visible) {
            continue;
        }

        bool const is_stacked = (data.display_options->display_mode == EventDisplayMode::Stacked);
        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, is_stacked);
    }

    // Add digital interval series (always full-canvas, non-stackable)
    for (auto const & [key, data]: _digital_interval_series) {
        if (!data.display_options->style.is_visible) {
            continue;
        }

        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalInterval, false);
    }

    return request;
}

void TimeSeriesDataStore::applyLayoutResponse(CorePlotting::LayoutResponse const & response) {
    for (auto const & layout: response.layouts) {
        // Find and update analog series
        auto analog_it = _analog_series.find(layout.series_id);
        if (analog_it != _analog_series.end()) {
            analog_it->second.display_options->layout_transform = layout.y_transform;
            continue;
        }

        // Find and update digital event series
        auto event_it = _digital_event_series.find(layout.series_id);
        if (event_it != _digital_event_series.end()) {
            event_it->second.display_options->layout_transform = layout.y_transform;
            continue;
        }

        // Find and update digital interval series
        auto interval_it = _digital_interval_series.find(layout.series_id);
        if (interval_it != _digital_interval_series.end()) {
            interval_it->second.display_options->layout_transform = layout.y_transform;
            continue;
        }
    }
}

// ============================================================================
// Series Lookup
// ============================================================================

SeriesType TimeSeriesDataStore::findSeriesTypeByKey(std::string const & key) const {
    if (_analog_series.contains(key)) {
        return SeriesType::Analog;
    }
    if (_digital_event_series.contains(key)) {
        return SeriesType::DigitalEvent;
    }
    if (_digital_interval_series.contains(key)) {
        return SeriesType::DigitalInterval;
    }
    return SeriesType::None;
}

bool TimeSeriesDataStore::isEmpty() const {
    return _analog_series.empty() &&
           _digital_event_series.empty() &&
           _digital_interval_series.empty();
}

size_t TimeSeriesDataStore::totalSeriesCount() const {
    return _analog_series.size() +
           _digital_event_series.size() +
           _digital_interval_series.size();
}

}// namespace DataViewer
