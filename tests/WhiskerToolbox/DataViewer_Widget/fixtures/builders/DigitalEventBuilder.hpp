#ifndef DIGITALEVENTBUILDER_HPP
#define DIGITALEVENTBUILDER_HPP

#include "../../../../../src/WhiskerToolbox/DataViewer/DigitalEvent/MVP_DigitalEvent.hpp"
#include "../../../../../src/WhiskerToolbox/DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"

#include <vector>
#include <random>
#include <algorithm>

/**
 * @brief Builder for creating test EventData vectors
 * 
 * Provides fluent API for constructing digital event test data without
 * DataManager dependency. Useful for unit testing MVP matrix calculations
 * and display option configuration.
 */
class DigitalEventBuilder {
public:
    DigitalEventBuilder() = default;

    /**
     * @brief Generate random events uniformly distributed in time range
     * @param num_events Number of events to generate
     * @param max_time Maximum time value (default 10000.0f)
     * @param seed Random seed for reproducibility (default 42)
     * @return Reference to this builder for chaining
     */
    DigitalEventBuilder& withRandomEvents(size_t num_events, float max_time = 10000.0f, unsigned int seed = 42) {
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dist(0.0f, max_time);
        
        _events.clear();
        _events.reserve(num_events);
        
        for (size_t i = 0; i < num_events; ++i) {
            _events.emplace_back(dist(gen));
        }
        
        // Sort events by time for optimal visualization
        std::sort(_events.begin(), _events.end(), 
                  [](EventData const& a, EventData const& b) { return a.time < b.time; });
        
        return *this;
    }

    /**
     * @brief Add events at specific times
     * @param times Vector of event times
     * @return Reference to this builder for chaining
     */
    DigitalEventBuilder& withTimes(std::vector<float> const& times) {
        _events.clear();
        _events.reserve(times.size());
        
        for (float t : times) {
            _events.emplace_back(t);
        }
        
        return *this;
    }

    /**
     * @brief Add a single event at specified time
     * @param time Event time
     * @return Reference to this builder for chaining
     */
    DigitalEventBuilder& addEvent(float time) {
        _events.emplace_back(time);
        return *this;
    }

    /**
     * @brief Generate regularly spaced events
     * @param start_time Starting time
     * @param end_time Ending time
     * @param interval Time between events
     * @return Reference to this builder for chaining
     */
    DigitalEventBuilder& withRegularEvents(float start_time, float end_time, float interval) {
        _events.clear();
        
        for (float t = start_time; t <= end_time; t += interval) {
            _events.emplace_back(t);
        }
        
        return *this;
    }

    /**
     * @brief Build the event vector
     * @return Vector of EventData
     */
    std::vector<EventData> build() const {
        return _events;
    }

private:
    std::vector<EventData> _events;
};

/**
 * @brief Builder for creating digital event display options
 * 
 * Provides fluent API for constructing NewDigitalEventSeriesDisplayOptions
 * with common test configurations.
 */
class DigitalEventDisplayOptionsBuilder {
public:
    DigitalEventDisplayOptionsBuilder() {
        // Set reasonable defaults
        _options.plotting_mode = EventPlottingMode::Stacked;
        _options.display_mode = EventDisplayMode::Stacked;
        _options.alpha = 0.8f;
        _options.line_thickness = 2;
        _options.event_height = 0.1f;
        _options.margin_factor = 0.95f;
        _options.is_visible = true;
    }

    DigitalEventDisplayOptionsBuilder& withStackedMode() {
        _options.plotting_mode = EventPlottingMode::Stacked;
        return *this;
    }

    DigitalEventDisplayOptionsBuilder& withFullCanvasMode() {
        _options.plotting_mode = EventPlottingMode::FullCanvas;
        return *this;
    }

    DigitalEventDisplayOptionsBuilder& withAllocation(float center, float height) {
        _options.allocated_y_center = center;
        _options.allocated_height = height;
        return *this;
    }

    DigitalEventDisplayOptionsBuilder& withMarginFactor(float margin) {
        _options.margin_factor = margin;
        return *this;
    }

    DigitalEventDisplayOptionsBuilder& withAlpha(float alpha) {
        _options.alpha = alpha;
        return *this;
    }

    DigitalEventDisplayOptionsBuilder& withLineThickness(int thickness) {
        _options.line_thickness = thickness;
        return *this;
    }

    DigitalEventDisplayOptionsBuilder& withEventHeight(float height) {
        _options.event_height = height;
        return *this;
    }

    DigitalEventDisplayOptionsBuilder& withVisibility(bool visible) {
        _options.is_visible = visible;
        return *this;
    }

    /**
     * @brief Apply intrinsic properties based on event data
     * @param events Event data to analyze
     * @return Reference to this builder for chaining
     */
    DigitalEventDisplayOptionsBuilder& withIntrinsicProperties(std::vector<EventData> const& events) {
        if (events.empty()) {
            return *this;
        }

        size_t const num_events = events.size();

        // Reduce alpha for dense event series to prevent visual clutter
        if (num_events > 100) {
            float const density_factor = std::min(1.0f, 100.0f / static_cast<float>(num_events));
            _options.alpha = std::max(0.2f, 0.8f * density_factor);
        }

        // Reduce line thickness for very dense series
        if (num_events > 200) {
            _options.line_thickness = 1;
        }

        return *this;
    }

    NewDigitalEventSeriesDisplayOptions build() const {
        return _options;
    }

private:
    NewDigitalEventSeriesDisplayOptions _options;
};

#endif // DIGITALEVENTBUILDER_HPP
