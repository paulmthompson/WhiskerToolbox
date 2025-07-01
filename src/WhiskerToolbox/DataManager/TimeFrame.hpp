#ifndef TIMEFRAME_HPP
#define TIMEFRAME_HPP

#include <concepts>
#include <iostream>
#include <memory>
#include <vector>


struct TimeFrameIndex {
    explicit TimeFrameIndex(int64_t val)
        : value(val) {}

    [[nodiscard]] int64_t getValue() const {
        return value;
    }

    bool operator==(TimeFrameIndex const & other) const {
        return value == other.value;
    }

    bool operator!=(TimeFrameIndex const & other) const {
        return value != other.value;
    }

    bool operator<(TimeFrameIndex const & other) const {
        return value < other.value;
    }

    bool operator>(TimeFrameIndex const & other) const {
        return value > other.value;
    }

    bool operator<=(TimeFrameIndex const & other) const {
        return value <= other.value;
    }

    bool operator>=(TimeFrameIndex const & other) const {
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

    [[nodiscard]] TimeFrameIndex getIndexAtTime(float time) const;

    [[nodiscard]] int checkFrameInbounds(int frame_id) const;

protected:
private:
    std::vector<int> _times;
    int _total_frame_count{0};
};

/**
 * @brief Converts a time index from one TimeFrame to another.
 *
 * If the time value's original frame (`source_time_frame`) is the same as the series'
 * data frame (`destination_time_frame`), the time value is returned as is.
 * Otherwise, the time value (which is a time in `source_time_frame`) is converted
 * to an index within the `destination_time_frame`.
 *
 * @param time_value_in_source_frame The time value, expressed in `source_value_frame` coordinates.
 * @param source_time_frame The TimeFrame in which `time_value_in_source_frame` is defined.
 * @param destination_time_frame The TimeFrame associated with the data series being queried. This
 *                          also serves as the target frame for index conversion if frames differ.
 * @return The original `time_value_in_source_frame` if frames are the same object instance,
 *         or the corresponding index in `destination_time_frame` if frames are different.
 */
TimeFrameIndex getTimeIndexForSeries(TimeFrameIndex source_index,
                              TimeFrame const * source_time_frame,
                              TimeFrame const * destination_time_frame);

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
