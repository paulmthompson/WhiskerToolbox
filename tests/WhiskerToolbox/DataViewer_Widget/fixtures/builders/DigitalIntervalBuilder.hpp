#ifndef DIGITALINTERVALBUILDER_HPP
#define DIGITALINTERVALBUILDER_HPP

#include "../../../../../src/WhiskerToolbox/DataViewer/DigitalInterval/MVP_DigitalInterval.hpp"
#include "../../../../../src/WhiskerToolbox/DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "../../../../../src/TimeFrame/interval_data.hpp"

#include <vector>
#include <random>
#include <algorithm>

/**
 * @brief Builder for creating test Interval vectors
 * 
 * Provides fluent API for constructing digital interval test data without
 * DataManager dependency. Useful for unit testing MVP matrix calculations
 * and display option configuration.
 */
class DigitalIntervalBuilder {
public:
    DigitalIntervalBuilder() = default;

    /**
     * @brief Generate random intervals uniformly distributed in time range
     * @param num_intervals Number of intervals to generate
     * @param max_time Maximum time value (default 10000.0f)
     * @param min_duration Minimum interval duration (default 50.0f)
     * @param max_duration Maximum interval duration (default 500.0f)
     * @param seed Random seed for reproducibility (default 42)
     * @return Reference to this builder for chaining
     */
    DigitalIntervalBuilder& withRandomIntervals(size_t num_intervals,
                                                float max_time = 10000.0f,
                                                float min_duration = 50.0f,
                                                float max_duration = 500.0f,
                                                unsigned int seed = 42) {
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> time_dist(0.0f, max_time - max_duration);
        std::uniform_real_distribution<float> duration_dist(min_duration, max_duration);
        
        _intervals.clear();
        _intervals.reserve(num_intervals);
        
        for (size_t i = 0; i < num_intervals; ++i) {
            float start = time_dist(gen);
            float duration = duration_dist(gen);
            float end = start + duration;
            
            // Ensure end doesn't exceed max_time
            if (end > max_time) {
                end = max_time;
            }
            
            _intervals.emplace_back(static_cast<long>(start), static_cast<long>(end));
        }
        
        // Sort intervals by start time for optimal visualization
        std::sort(_intervals.begin(), _intervals.end(),
                  [](Interval const& a, Interval const& b) { return a.start < b.start; });
        
        return *this;
    }

    /**
     * @brief Add intervals at specific times
     * @param intervals Vector of intervals
     * @return Reference to this builder for chaining
     */
    DigitalIntervalBuilder& withIntervals(std::vector<Interval> const& intervals) {
        _intervals = intervals;
        return *this;
    }

    /**
     * @brief Add a single interval
     * @param start Interval start time
     * @param end Interval end time
     * @return Reference to this builder for chaining
     */
    DigitalIntervalBuilder& addInterval(long start, long end) {
        _intervals.emplace_back(start, end);
        return *this;
    }

    /**
     * @brief Generate regularly spaced intervals
     * @param start_time Starting time
     * @param end_time Ending time
     * @param spacing Time between interval starts
     * @param duration Duration of each interval
     * @return Reference to this builder for chaining
     */
    DigitalIntervalBuilder& withRegularIntervals(float start_time, float end_time, 
                                                 float spacing, float duration) {
        _intervals.clear();
        
        for (float t = start_time; t <= end_time; t += spacing) {
            long start = static_cast<long>(t);
            long end = static_cast<long>(t + duration);
            _intervals.emplace_back(start, end);
        }
        
        return *this;
    }

    /**
     * @brief Generate non-overlapping intervals
     * @param num_intervals Number of intervals to generate
     * @param max_time Maximum time value
     * @param min_gap Minimum gap between intervals
     * @param duration Fixed duration for each interval
     * @param seed Random seed for reproducibility
     * @return Reference to this builder for chaining
     */
    DigitalIntervalBuilder& withNonOverlappingIntervals(size_t num_intervals,
                                                        float max_time = 10000.0f,
                                                        float min_gap = 100.0f,
                                                        float duration = 200.0f,
                                                        unsigned int seed = 42) {
        _intervals.clear();
        
        if (num_intervals == 0) {
            return *this;
        }
        
        float required_time = num_intervals * (duration + min_gap);
        if (required_time > max_time) {
            // Adjust to fit
            duration = (max_time - num_intervals * min_gap) / num_intervals;
        }
        
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> gap_dist(min_gap, min_gap * 2.0f);
        
        float current_time = 0.0f;
        for (size_t i = 0; i < num_intervals; ++i) {
            long start = static_cast<long>(current_time);
            long end = static_cast<long>(current_time + duration);
            
            if (end <= max_time) {
                _intervals.emplace_back(start, end);
            }
            
            current_time = end + gap_dist(gen);
        }
        
        return *this;
    }

    /**
     * @brief Build the interval vector
     * @return Vector of Interval
     */
    std::vector<Interval> build() const {
        return _intervals;
    }

private:
    std::vector<Interval> _intervals;
};

/**
 * @brief Builder for creating digital interval display options
 * 
 * Provides fluent API for constructing NewDigitalIntervalSeriesDisplayOptions
 * with common test configurations.
 */
class DigitalIntervalDisplayOptionsBuilder {
public:
    DigitalIntervalDisplayOptionsBuilder() {
        // Set reasonable defaults
        _options.alpha = 0.3f;
        _options.is_visible = true;
    }

    DigitalIntervalDisplayOptionsBuilder& withAlpha(float alpha) {
        _options.alpha = alpha;
        return *this;
    }

    DigitalIntervalDisplayOptionsBuilder& withVisibility(bool visible) {
        _options.is_visible = visible;
        return *this;
    }

    /**
     * @brief Apply intrinsic properties based on interval data
     * @param intervals Interval data to analyze
     * @return Reference to this builder for chaining
     */
    DigitalIntervalDisplayOptionsBuilder& withIntrinsicProperties(std::vector<Interval> const& intervals) {
        if (intervals.empty()) {
            return *this;
        }

        // Analyze interval overlap and density
        size_t max_overlap = 0;
        for (size_t i = 0; i < intervals.size(); ++i) {
            size_t overlap = 0;
            for (size_t j = 0; j < intervals.size(); ++j) {
                if (i != j) {
                    // Check if intervals overlap
                    if (intervals[i].start < intervals[j].end && intervals[i].end > intervals[j].start) {
                        overlap++;
                    }
                }
            }
            max_overlap = std::max(max_overlap, overlap);
        }

        // Adjust alpha based on overlap density
        if (max_overlap > 5) {
            _options.alpha = 0.15f;
        } else if (max_overlap > 2) {
            _options.alpha = 0.2f;
        } else if (max_overlap > 0) {
            _options.alpha = 0.25f;
        } else {
            _options.alpha = 0.3f;
        }

        return *this;
    }

    NewDigitalIntervalSeriesDisplayOptions build() const {
        return _options;
    }

private:
    NewDigitalIntervalSeriesDisplayOptions _options;
};

#endif // DIGITALINTERVALBUILDER_HPP
