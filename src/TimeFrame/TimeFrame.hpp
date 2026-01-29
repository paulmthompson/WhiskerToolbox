#ifndef TIMEFRAME_HPP
#define TIMEFRAME_HPP

#include <concepts>
#include <iostream>
#include <memory>
#include <vector>


struct TimeFrameIndex {
    explicit constexpr TimeFrameIndex(int64_t val)
        : value(val) {}

    [[nodiscard]] constexpr int64_t getValue() const {
        return value;
    }

    constexpr bool operator==(TimeFrameIndex const & other) const {
        return value == other.value;
    }

    constexpr bool operator!=(TimeFrameIndex const & other) const {
        return value != other.value;
    }

    constexpr bool operator<(TimeFrameIndex const & other) const {
        return value < other.value;
    }

    constexpr bool operator>(TimeFrameIndex const & other) const {
        return value > other.value;
    }

    constexpr bool operator<=(TimeFrameIndex const & other) const {
        return value <= other.value;
    }

    constexpr bool operator>=(TimeFrameIndex const & other) const {
        return value >= other.value;
    }

    TimeFrameIndex & operator++() {
        ++value;
        return *this;
    }

    TimeFrameIndex operator++(int) {
        TimeFrameIndex temp(*this);
        ++value;
        return temp;
    }

    //Arithmetic operations
    TimeFrameIndex operator+(TimeFrameIndex const & other) const {
        return TimeFrameIndex(value + other.value);
    }

    TimeFrameIndex operator-(TimeFrameIndex const & other) const {
        return TimeFrameIndex(value - other.value);
    }


private:
    int64_t value;
};

class TimeFrame {
public:
    TimeFrame() = default;
    explicit TimeFrame(std::vector<int> const & times);

    [[nodiscard]] int getTotalFrameCount() const { return _total_frame_count; };

    [[nodiscard]] int getTimeAtIndex(TimeFrameIndex index) const;

    [[nodiscard]] TimeFrameIndex getIndexAtTime(float time, bool preceding = true) const;

    [[nodiscard]] int checkFrameInbounds(int frame_id) const;

protected:
private:
    std::vector<int> _times;
    int _total_frame_count{0};
};

//TimeFrameIndex and TimeFrame struct
struct TimeIndexAndFrame {
    TimeFrameIndex index;
    TimeFrame const * const time_frame;

    TimeIndexAndFrame(int64_t index_value, TimeFrame const * time_frame_ptr)
        : index(TimeFrameIndex(index_value)),
          time_frame(time_frame_ptr) {}

    TimeIndexAndFrame(TimeFrameIndex time_frame_index, TimeFrame const * time_frame_ptr)
        : index(time_frame_index),
          time_frame(time_frame_ptr) {}
};

/**
 * @brief Converts a time index range from one TimeFrame to another.
 *
 * This function takes a range defined by start and stop indices in the source
 * timeframe and converts it to the corresponding range in the target timeframe.
 * The conversion is done by:
 * 1. Getting the time values at the source indices
 * 2. Finding the corresponding indices in the target timeframe
 *
 * @param start_index The start index in the source timeframe
 * @param stop_index The stop index in the source timeframe
 * @param from_time_frame The source timeframe containing the input range
 * @param to_time_frame The target timeframe for the converted range
 * @return A pair of TimeFrameIndex representing the converted start and stop indices
 */
[[nodiscard]] std::pair<TimeFrameIndex, TimeFrameIndex> convertTimeFrameRange(
        TimeFrameIndex start_index,
        TimeFrameIndex stop_index,
        TimeFrame const & from_time_frame,
        TimeFrame const & to_time_frame);

/**
 * @brief Convert a time index from one TimeFrame to another.
 *
 * @param time The time index to convert.
 * @param source_timeframe The source TimeFrame.
 * @param target_timeframe The target TimeFrame.
 * @return The converted time index.
 */
TimeFrameIndex convert_time_index(TimeFrameIndex const time,
                                  TimeFrame const * source_timeframe,
                                  TimeFrame const * target_timeframe);

/**
 * @brief Position in time with clock identity
 * 
 * Combines a TimeFrameIndex with the TimeFrame it belongs to.
 * This is the primary type for time change signals because it allows:
 * - Pointer comparison to check if two positions are on the same clock
 * - Direct index conversion between different TimeFrames
 * - Safe passage through Qt signals (shared_ptr keeps TimeFrame alive)
 * 
 * @note Uses shared_ptr for signal safety (TimeFrame outlives async signals)
 */
struct TimePosition {
    TimeFrameIndex index{0};
    std::shared_ptr<TimeFrame> time_frame;

    TimePosition() = default;

    TimePosition(TimeFrameIndex idx, std::shared_ptr<TimeFrame> tf)
        : index(idx),
          time_frame(std::move(tf)) {}

    explicit TimePosition(int64_t idx, std::shared_ptr<TimeFrame> tf = nullptr)
        : index(TimeFrameIndex(idx)),
          time_frame(std::move(tf)) {}

    /// Check if two positions are on the same clock (pointer comparison)
    [[nodiscard]] bool sameClock(TimePosition const & other) const {
        return time_frame.get() == other.time_frame.get();
    }

    /// Check against a raw TimeFrame pointer (e.g., from data->getTimeFrame().get())
    [[nodiscard]] bool sameClock(TimeFrame const * other) const {
        return time_frame.get() == other;
    }

    /// Check against a shared_ptr (e.g., from data->getTimeFrame())
    [[nodiscard]] bool sameClock(std::shared_ptr<TimeFrame> const & other) const {
        return time_frame.get() == other.get();
    }

    /// Convert this position to a different timeframe
    [[nodiscard]] TimeFrameIndex convertTo(TimeFrame const * target) const {
        if (!time_frame || !target || time_frame.get() == target) {
            return index;
        }
        return convert_time_index(index, time_frame.get(), target);
    }

    /// Convert to a shared_ptr timeframe
    [[nodiscard]] TimeFrameIndex convertTo(std::shared_ptr<TimeFrame> const & target) const {
        return convertTo(target.get());
    }

    /// Check if this position has a valid TimeFrame
    [[nodiscard]] bool isValid() const {
        return time_frame != nullptr;
    }

    /// Equality (same clock AND same index)
    bool operator==(TimePosition const & other) const {
        return sameClock(other) && index == other.index;
    }

    bool operator!=(TimePosition const & other) const {
        return !(*this == other);
    }
};


// ========== Filename-based TimeFrame Creation ==========

/**
 * @brief Mode for creating TimeFrame from filename-extracted values
 */
enum class FilenameTimeFrameMode {
    FOUND_VALUES,///< Use only the values found in filenames (sparse)
    ZERO_TO_MAX, ///< Create range from 0 to maximum found value (dense)
    MIN_TO_MAX   ///< Create range from minimum to maximum found value (dense)
};

/**
 * @brief Options for creating TimeFrame from image folder filenames
 */
struct FilenameTimeFrameOptions {
    std::string folder_path;   ///< Path to the folder containing files
    std::string file_extension;///< File extension to filter (e.g., ".jpg", ".png")
    std::string regex_pattern; ///< Regex pattern to extract numerical values from filenames
    FilenameTimeFrameMode mode;///< Mode for TimeFrame creation
    bool sort_ascending = true;///< Whether to sort extracted values in ascending order
};

/**
 * @brief Create a legacy TimeFrame from image folder filenames
 * 
 * This function scans a folder for files with a specific extension, extracts numerical
 * values from their filenames using a regex pattern, and creates a TimeFrame based on
 * the specified mode.
 * 
 * @param options Configuration options for the operation
 * @return A shared pointer to the created TimeFrame, or nullptr on failure
 * 
 * @note Files with no extractable numbers or invalid patterns are skipped with warnings
 * @note The regex pattern should contain exactly one capture group for the numerical value
 * 
 * @example
 * ```cpp
 * FilenameTimeFrameOptions opts;
 * opts.folder_path = "/path/to/images";
 * opts.file_extension = ".jpg";
 * opts.regex_pattern = R"(frame_(\d+)\.jpg)";  // Captures number after "frame_"
 * opts.mode = FilenameTimeFrameMode::FOUND_VALUES;
 * 
 * auto timeframe = createTimeFrameFromFilenames(opts);
 * ```
 */
std::shared_ptr<TimeFrame> createTimeFrameFromFilenames(FilenameTimeFrameOptions const & options);


#endif// TIMEFRAME_HPP
