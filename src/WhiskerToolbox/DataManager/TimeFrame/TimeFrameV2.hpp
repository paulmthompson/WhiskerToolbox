#ifndef TIMEFRAME_V2_HPP
#define TIMEFRAME_V2_HPP

#include "StrongTimeTypes.hpp"
#include "TimeFrame.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <variant>
#include <vector>

// Forward declarations for filename-based creation
enum class FilenameTimeFrameMode;
struct FilenameTimeFrameOptions;

/**
 * @brief Dense time range representation using std::ranges::iota_view
 * 
 * Represents a regular sequence of time values without storing them explicitly.
 * Useful for dense sampling scenarios like electrophysiology data.
 */
struct DenseTimeRange {
    int64_t start;
    int64_t count;
    int64_t step;

    DenseTimeRange(int64_t start_val, int64_t count_val, int64_t step_val = 1)
        : start(start_val),
          count(count_val),
          step(step_val) {}

    /**
     * @brief Get the time value at a specific index
     * 
     * @param index Index into the dense range
     * @return Time value at the given index
     */
    [[nodiscard]] int64_t getTimeAtIndex(TimeFrameIndex index) const {
        if (index.getValue() < 0 || index.getValue() >= count) {
            return start;// Return start value for out-of-bounds access
        }
        return start + index.getValue() * step;
    }

    /**
     * @brief Find the index closest to a given time value
     * 
     * @param time_value Time value to search for
     * @return Index of the closest time value
     */
    [[nodiscard]] TimeFrameIndex getIndexAtTime(double time_value) const {
        if (count == 0) return TimeFrameIndex(0);

        TimeFrameIndex index = TimeFrameIndex(static_cast<int64_t>((time_value - start) / step));

        // Clamp to valid range
        if (index.getValue() < 0) return TimeFrameIndex(0);
        if (index.getValue() >= count) return TimeFrameIndex(count - 1);

        return index;
    }

    /**
     * @brief Get the total number of time points
     */
    [[nodiscard]] int64_t size() const { return count; }
};

/**
 * @brief Storage variant for TimeFrame data
 * 
 * Can hold either sparse time data (std::vector) or dense time data (DenseTimeRange).
 */
using TimeStorage = std::variant<std::vector<int64_t>, DenseTimeRange>;

/**
 * @brief Enhanced TimeFrame with variant storage and strong type support
 * 
 * This is the new TimeFrame implementation that supports both sparse and dense
 * time storage, strong typing for different coordinate systems, and efficient
 * memory usage for regular sampling patterns.
 */
template<typename CoordinateType>
class TimeFrameV2 {
    using TimeStorage = std::variant<std::vector<int64_t>, DenseTimeRange>;

public:
    // Type alias to expose template parameter
    using coordinate_type = CoordinateType;
    /**
     * @brief Create a TimeFrame from a vector of time values (sparse storage)
     * 
     * @param times Vector of time values
     * @param sampling_rate_hz Optional sampling rate in Hz (for ClockTicks conversion to seconds)
     */
    explicit TimeFrameV2(std::vector<int64_t> times, std::optional<double> sampling_rate_hz = std::nullopt)
        : _storage(std::move(times)),
          _sampling_rate_hz(sampling_rate_hz) {}

    /**
     * @brief Create a TimeFrame with dense storage
     * 
     * @param start Starting time value
     * @param count Number of time points
     * @param step Step size between consecutive time points
     * @param sampling_rate_hz Optional sampling rate in Hz (for ClockTicks conversion to seconds)
     */
    TimeFrameV2(int64_t start, int64_t count, int64_t step = 1, std::optional<double> sampling_rate_hz = std::nullopt)
        : _storage(DenseTimeRange(start, count, step)),
          _sampling_rate_hz(sampling_rate_hz) {}

    /**
     * @brief Get the time value at a specific index
     * 
     * @param index Index into the time frame
     * @return Time coordinate at the given index
     */
    [[nodiscard]] CoordinateType getTimeAtIndex(TimeFrameIndex index) const {
        return std::visit([index](auto const & storage) -> CoordinateType {
            if constexpr (std::is_same_v<std::decay_t<decltype(storage)>, std::vector<int64_t>>) {
                if (index.getValue() < 0 || static_cast<size_t>(index.getValue()) >= storage.size()) {
                    return CoordinateType(0);// Return zero for out-of-bounds
                }
                return CoordinateType(storage[static_cast<size_t>(index.getValue())]);
            } else {// DenseTimeRange
                return CoordinateType(storage.getTimeAtIndex(index));
            }
        },
                          _storage);
    }

    /**
     * @brief Find the index closest to a given time value
     * 
     * @param time_value Time value to search for (in the same coordinate system)
     * @return Index of the closest time value
     */
    [[nodiscard]] TimeFrameIndex getIndexAtTime(CoordinateType time_value) const {
        return std::visit([time_value](auto const & storage) -> TimeFrameIndex {
            if constexpr (std::is_same_v<std::decay_t<decltype(storage)>, std::vector<int64_t>>) {
                // Binary search for sparse storage
                double target = static_cast<double>(time_value.getValue());
                auto it = std::lower_bound(storage.begin(), storage.end(), target);

                if (it == storage.end()) {
                    return TimeFrameIndex(static_cast<int64_t>(storage.size()) - 1);
                }
                if (it == storage.begin()) {
                    return TimeFrameIndex(0);
                }

                // Find closest value
                auto prev = it - 1;
                if (std::abs(static_cast<double>(*prev) - target) <= std::abs(static_cast<double>(*it) - target)) {
                    return TimeFrameIndex(static_cast<int64_t>(std::distance(storage.begin(), prev)));
                } else {
                    return TimeFrameIndex(static_cast<int64_t>(std::distance(storage.begin(), it)));
                }
            } else {// DenseTimeRange
                return storage.getIndexAtTime(static_cast<double>(time_value.getValue()));
            }
        },
                          _storage);
    }

    /**
     * @brief Get the total number of time points
     */
    [[nodiscard]] int64_t getTotalFrameCount() const {
        return std::visit([](auto const & storage) -> int64_t {
            if constexpr (std::is_same_v<std::decay_t<decltype(storage)>, std::vector<int64_t>>) {
                return static_cast<int64_t>(storage.size());
            } else {// DenseTimeRange
                return storage.size();
            }
        },
                          _storage);
    }

    /**
     * @brief Check if an index is within bounds and clamp if necessary
     * 
     * @param index Index to check
     * @return Clamped index within valid range
     */
    [[nodiscard]] TimeFrameIndex checkIndexInBounds(TimeFrameIndex index) const {
        int64_t total_count = getTotalFrameCount();
        if (index.getValue() < 0) return TimeFrameIndex(0);
        if (index.getValue() >= total_count) return TimeFrameIndex(total_count - 1);
        return TimeFrameIndex(index.getValue());
    }

    /**
     * @brief Get the sampling rate (if available)
     * 
     * Only meaningful for ClockTicks coordinate types.
     * 
     * @return Sampling rate in Hz, or nullopt if not set
     */
    [[nodiscard]] std::optional<double> getSamplingRate() const { return _sampling_rate_hz; }

    /**
     * @brief Convert coordinate to seconds (if sampling rate is available)
     * 
     * Only works for ClockTicks coordinate types with a known sampling rate.
     * 
     * @param coord Coordinate to convert
     * @return Time in seconds, or nullopt if conversion is not possible
     */
    [[nodiscard]] std::optional<Seconds> coordinateToSeconds(CoordinateType coord) const {
        if constexpr (std::is_same_v<CoordinateType, ClockTicks>) {
            if (_sampling_rate_hz.has_value()) {
                return Seconds(static_cast<double>(coord.getValue()) / _sampling_rate_hz.value());
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Convert seconds to coordinate (if sampling rate is available)
     * 
     * Only works for ClockTicks coordinate types with a known sampling rate.
     * 
     * @param seconds Time in seconds
     * @return Coordinate value, or nullopt if conversion is not possible
     */
    [[nodiscard]] std::optional<CoordinateType> secondsToCoordinate(Seconds seconds) const {
        if constexpr (std::is_same_v<CoordinateType, ClockTicks>) {
            if (_sampling_rate_hz.has_value()) {
                int64_t ticks = static_cast<int64_t>(seconds.getValue() * _sampling_rate_hz.value());
                return CoordinateType(ticks);
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Check if this TimeFrame uses dense storage
     */
    [[nodiscard]] bool isDense() const {
        return std::holds_alternative<DenseTimeRange>(_storage);
    }

    /**
     * @brief Check if this TimeFrame uses sparse storage
     */
    [[nodiscard]] bool isSparse() const {
        return std::holds_alternative<std::vector<int64_t>>(_storage);
    }

private:
    TimeStorage _storage;
    std::optional<double> _sampling_rate_hz;
};

// Type aliases for common TimeFrame types
using CameraTimeFrame = TimeFrameV2<CameraFrameIndex>;
using ClockTimeFrame = TimeFrameV2<ClockTicks>;
using SecondsTimeFrame = TimeFrameV2<Seconds>;
using UncalibratedTimeFrame = TimeFrameV2<UncalibratedIndex>;

/**
 * @brief Variant type that can hold any TimeFrame type
 */
using AnyTimeFrame = std::variant<
        std::shared_ptr<CameraTimeFrame>,
        std::shared_ptr<ClockTimeFrame>,
        std::shared_ptr<SecondsTimeFrame>,
        std::shared_ptr<UncalibratedTimeFrame>>;

/**
 * @brief Utility functions for TimeFrame operations
 */
namespace TimeFrameUtils {

/**
     * @brief Create a dense ClockTimeFrame for regular sampling
     * 
     * @param start_tick Starting tick value
     * @param num_samples Number of samples
     * @param sampling_rate_hz Sampling rate in Hz
     * @return Shared pointer to the created ClockTimeFrame
     */
[[nodiscard]] inline std::shared_ptr<ClockTimeFrame> createDenseClockTimeFrame(
        int64_t start_tick, int64_t num_samples, double sampling_rate_hz) {
    return std::make_shared<ClockTimeFrame>(start_tick, num_samples, 1, sampling_rate_hz);
}

/**
     * @brief Create a sparse ClockTimeFrame from tick indices
     * 
     * @param tick_indices Vector of clock tick indices
     * @param sampling_rate_hz Sampling rate in Hz
     * @return Shared pointer to the created ClockTimeFrame
     */
[[nodiscard]] inline std::shared_ptr<ClockTimeFrame> createSparseClockTimeFrame(
        std::vector<int64_t> tick_indices, double sampling_rate_hz) {
    return std::make_shared<ClockTimeFrame>(std::move(tick_indices), sampling_rate_hz);
}

/**
     * @brief Create a sparse CameraTimeFrame from frame indices
     * 
     * @param frame_indices Vector of camera frame indices
     * @return Shared pointer to the created CameraTimeFrame
     */
[[nodiscard]] inline std::shared_ptr<CameraTimeFrame> createSparseCameraTimeFrame(
        std::vector<int64_t> frame_indices) {
    return std::make_shared<CameraTimeFrame>(std::move(frame_indices));
}

/**
     * @brief Create a dense CameraTimeFrame for regular frame capture
     * 
     * @param start_frame Starting frame index
     * @param num_frames Number of frames
     * @return Shared pointer to the created CameraTimeFrame
     */
[[nodiscard]] inline std::shared_ptr<CameraTimeFrame> createDenseCameraTimeFrame(
        int64_t start_frame, int64_t num_frames) {
    return std::make_shared<CameraTimeFrame>(start_frame, num_frames, 1);
}

// ========== Filename-based TimeFrameV2 Creation ==========

/**
     * @brief Create a CameraTimeFrame from image folder filenames
     * 
     * This function is the TimeFrameV2 equivalent of createTimeFrameFromFilenames.
     * It extracts frame indices from filenames and creates a CameraTimeFrame.
     * 
     * @param options Configuration options (same as legacy version)
     * @return A shared pointer to the created CameraTimeFrame, or nullptr on failure
     */
[[nodiscard]] std::shared_ptr<CameraTimeFrame> createCameraTimeFrameFromFilenames(
        FilenameTimeFrameOptions const & options);

/**
     * @brief Create an UncalibratedTimeFrame from image folder filenames
     * 
     * For when the extracted values don't represent camera frames but some other
     * uncalibrated indexing scheme.
     * 
     * @param options Configuration options (same as legacy version)
     * @return A shared pointer to the created UncalibratedTimeFrame, or nullptr on failure
     */
[[nodiscard]] std::shared_ptr<UncalibratedTimeFrame> createUncalibratedTimeFrameFromFilenames(
        FilenameTimeFrameOptions const & options);
}// namespace TimeFrameUtils

#endif// TIMEFRAME_V2_HPP