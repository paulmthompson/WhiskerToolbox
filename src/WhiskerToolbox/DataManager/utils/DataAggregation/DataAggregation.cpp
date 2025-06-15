#include "DataAggregation.hpp"

#include "AnalogTimeSeries/utils/statistics.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace DataAggregation {

int64_t calculateOverlapDuration(Interval const & a, Interval const & b) {
    const int64_t overlap_start = std::max(a.start, b.start);
    const int64_t overlap_end = std::min(a.end, b.end);

    if (overlap_start <= overlap_end) {
        return overlap_end - overlap_start + 1;
    }
    return 0;
}

int findOverlappingIntervalIndex(Interval const & target_interval,
                                 std::vector<Interval> const & reference_intervals,
                                 OverlapStrategy strategy) {
    std::vector<int> overlapping_indices;

    // Find all overlapping intervals using existing is_overlapping function
    for (size_t i = 0; i < reference_intervals.size(); ++i) {
        if (is_overlapping(target_interval, reference_intervals[i])) {
            overlapping_indices.push_back(static_cast<int>(i));
        }
    }

    if (overlapping_indices.empty()) {
        return -1;// No overlap found
    }

    switch (strategy) {
        case OverlapStrategy::First:
            return overlapping_indices.front();

        case OverlapStrategy::Last:
            return overlapping_indices.back();

        case OverlapStrategy::MaxOverlap: {
            int best_index = overlapping_indices[0];
            int64_t max_overlap = calculateOverlapDuration(target_interval, reference_intervals[best_index]);

            for (size_t i = 1; i < overlapping_indices.size(); ++i) {
                const int current_index = overlapping_indices[i];
                const int64_t current_overlap = calculateOverlapDuration(target_interval, reference_intervals[current_index]);

                if (current_overlap > max_overlap) {
                    max_overlap = current_overlap;
                    best_index = current_index;
                }
            }
            return best_index;
        }

        default:
            throw std::invalid_argument("Unknown overlap strategy");
    }
}

double applyTransformation(Interval const & interval,
                           TransformationConfig const & config,
                           std::map<std::string, std::vector<Interval>> const & reference_intervals,
                           std::map<std::string, std::shared_ptr<AnalogTimeSeries>> const & reference_analog,
                           std::map<std::string, std::shared_ptr<PointData>> const & reference_points) {
    switch (config.type) {
        case TransformationType::IntervalStart:
            return static_cast<double>(interval.start);

        case TransformationType::IntervalEnd:
            return static_cast<double>(interval.end);

        case TransformationType::IntervalDuration:
            return static_cast<double>(interval.end - interval.start + 1);

        case TransformationType::IntervalID: {
            auto it = reference_intervals.find(config.reference_data_key);
            if (it == reference_intervals.end()) {
                return std::nan("");// Reference data not found
            }

            const int overlap_index = findOverlappingIntervalIndex(interval, it->second, config.overlap_strategy);
            if (overlap_index == -1) {
                return std::nan("");// No overlap found
            }

            return static_cast<double>(overlap_index);// 0-based indexing
        }

        case TransformationType::IntervalCount: {
            auto it = reference_intervals.find(config.reference_data_key);
            if (it == reference_intervals.end()) {
                return std::nan("");// Reference data not found
            }

            // Count all overlapping intervals
            int count = 0;
            for (auto const & ref_interval: it->second) {
                if (is_overlapping(interval, ref_interval)) {
                    count++;
                }
            }

            return static_cast<double>(count);
        }

        case TransformationType::AnalogMean: {
            auto it = reference_analog.find(config.reference_data_key);
            if (it == reference_analog.end() || !it->second) {
                return std::nan("");// Reference data not found
            }

            return calculate_mean_in_time_range(*it->second, TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }

        case TransformationType::AnalogMin: {
            auto it = reference_analog.find(config.reference_data_key);
            if (it == reference_analog.end() || !it->second) {
                return std::nan("");// Reference data not found
            }

            auto start_index = it->second->findDataArrayIndexGreaterOrEqual(TimeFrameIndex(interval.start));
            auto end_index = it->second->findDataArrayIndexLessOrEqual(TimeFrameIndex(interval.end));

            if (!start_index.has_value() || !end_index.has_value()) {
                return std::nan("");// No data found in interval
            }

            // Get data within the time interval
            return calculate_min(*it->second, start_index.value().getValue(), end_index.value().getValue() + 1); // exclusive end
        }

        case TransformationType::AnalogMax: {
            auto it = reference_analog.find(config.reference_data_key);
            if (it == reference_analog.end() || !it->second) {
                return std::nan("");// Reference data not found
            }

            auto start_index = it->second->findDataArrayIndexGreaterOrEqual(TimeFrameIndex(interval.start));
            auto end_index = it->second->findDataArrayIndexLessOrEqual(TimeFrameIndex(interval.end));

            if (!start_index.has_value() || !end_index.has_value()) {
                return std::nan("");// No data found in interval
            }

            // Get data within the time interval
            return calculate_max(*it->second, start_index.value().getValue(), end_index.value().getValue() + 1); // exclusive end
        }

        case TransformationType::AnalogStdDev: {
            auto it = reference_analog.find(config.reference_data_key);
            if (it == reference_analog.end() || !it->second) {
                return std::nan("");// Reference data not found
            }

            // Get data within the time interval
            auto start_index = it->second->findDataArrayIndexGreaterOrEqual(TimeFrameIndex(interval.start));
            auto end_index = it->second->findDataArrayIndexLessOrEqual(TimeFrameIndex(interval.end));

            if (!start_index.has_value() || !end_index.has_value()) {
                return std::nan("");// No data found in interval
            }

            return calculate_std_dev(*it->second, start_index.value().getValue(), end_index.value().getValue() + 1); // exclusive end
        }

        case TransformationType::PointMeanX: {
            auto it = reference_points.find(config.reference_data_key);
            if (it == reference_points.end() || !it->second) {
                return std::nan("");// Reference data not found
            }

            // Collect all X coordinates within the interval
            std::vector<float> x_values;
            for (int64_t t = interval.start; t <= interval.end; ++t) {
                const auto& points = it->second->getPointsAtTime(static_cast<int>(t));
                for (const auto& point : points) {
                    x_values.push_back(point.x);
                }
            }

            if (x_values.empty()) {
                return std::nan("");// No points found in interval
            }

            // Calculate mean
            double sum = 0.0;
            for (float x : x_values) {
                sum += static_cast<double>(x);
            }
            return sum / static_cast<double>(x_values.size());
        }

        case TransformationType::PointMeanY: {
            auto it = reference_points.find(config.reference_data_key);
            if (it == reference_points.end() || !it->second) {
                return std::nan("");// Reference data not found
            }

            // Collect all Y coordinates within the interval
            std::vector<float> y_values;
            for (int64_t t = interval.start; t <= interval.end; ++t) {
                const auto& points = it->second->getPointsAtTime(static_cast<int>(t));
                for (const auto& point : points) {
                    y_values.push_back(point.y);
                }
            }

            if (y_values.empty()) {
                return std::nan("");// No points found in interval
            }

            // Calculate mean
            double sum = 0.0;
            for (float y : y_values) {
                sum += static_cast<double>(y);
            }
            return sum / static_cast<double>(y_values.size());
        }

        default:
            throw std::invalid_argument("Unknown transformation type");
    }
}

std::vector<std::vector<double>> aggregateData(
        std::vector<Interval> const & row_intervals,
        std::vector<TransformationConfig> const & transformations,
        std::map<std::string, std::vector<Interval>> const & reference_intervals,
        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> const & reference_analog,
        std::map<std::string, std::shared_ptr<PointData>> const & reference_points) {

    std::vector<std::vector<double>> result;
    result.reserve(row_intervals.size());

    for (auto const & interval: row_intervals) {
        std::vector<double> row;
        row.reserve(transformations.size());

        for (auto const & transformation: transformations) {
            const double value = applyTransformation(interval, transformation, reference_intervals, reference_analog, reference_points);
            row.push_back(value);
        }

        result.push_back(std::move(row));
    }

    return result;
}

}// namespace DataAggregation