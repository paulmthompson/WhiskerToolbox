#ifndef TIMEFRAME_HPP
#define TIMEFRAME_HPP

#include <concepts>
#include <iostream>
#include <vector>

struct TimeIndex {
    explicit TimeIndex(int64_t val) : value(val) {}

    // Getter for the underlying value
    [[nodiscard]] int64_t getValue() const {
        return value;
    }

    // (Optional) Overload comparison operators if needed
    bool operator==(TimeIndex const& other) const {
        return value == other.value;
    }

    bool operator!=(TimeIndex const& other) const {
        return value != other.value;
    }

    bool operator<(TimeIndex const& other) const {
        return value < other.value;
    }
private:
    int64_t value;
};

class TimeFrame {
public:
    TimeFrame() = default;
    explicit TimeFrame(std::vector<int> const & times);

    [[nodiscard]] int getTotalFrameCount() const { return _total_frame_count; };

    [[nodiscard]] int getTimeAtIndex(TimeIndex index) const;

    [[nodiscard]] int getIndexAtTime(float time) const;

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
int64_t getTimeIndexForSeries(TimeIndex time_value_in_source_frame,
                                 TimeFrame const * source_time_frame,
                                 TimeFrame const * destination_time_frame);


#endif// TIMEFRAME_HPP
