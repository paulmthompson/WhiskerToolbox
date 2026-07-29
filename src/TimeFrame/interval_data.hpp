#ifndef INTERVAL_DATA_HPP
#define INTERVAL_DATA_HPP

#include "TimeFrame/ClockTicks.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cstdint>

template<typename T>
struct IntervalT {
    T start;
    T end;
    bool operator==(IntervalT const & other) const {
        return start == other.start && end == other.end;
    }
};

// Type aliases for backwards compatibility and common types
using Interval = IntervalT<int64_t>;
using TimeFrameInterval = IntervalT<TimeFrameIndex>;
using ClockTicksInterval = IntervalT<ClockTicks>;

template<typename T>
inline bool operator<(IntervalT<T> const & a, IntervalT<T> const & b) {
    return a.start < b.start;
}

template<typename T>
inline bool is_overlapping(IntervalT<T> const & a, IntervalT<T> const & b) {
    return a.start <= b.end && b.start <= a.end;
}

template<typename T>
inline bool is_contiguous(IntervalT<T> const & a, IntervalT<T> const & b) {
    return a.end + T(1) == b.start || b.end + T(1) == a.start;
}

template<typename T>
inline bool is_contained(IntervalT<T> const & a, IntervalT<T> const & b) {
    return a.start <= b.start && a.end >= b.end;
}

template<typename T>
inline bool is_contained(IntervalT<T> const & a, T const time) {
    return a.start <= time && time <= a.end;
}

//instantiate the template functions for the common types
template bool is_overlapping(Interval const & a, Interval const & b);
template bool is_contiguous(Interval const & a, Interval const & b);
template bool is_contained(Interval const & a, Interval const & b);
template bool is_contained(Interval const & a, int64_t const time);
template bool operator<(Interval const & a, Interval const & b);

template bool is_overlapping(TimeFrameInterval const & a, TimeFrameInterval const & b);
template bool is_contiguous(TimeFrameInterval const & a, TimeFrameInterval const & b);
template bool is_contained(TimeFrameInterval const & a, TimeFrameInterval const & b);
template bool is_contained(TimeFrameInterval const & a, TimeFrameIndex const time);
template bool operator<(TimeFrameInterval const & a, TimeFrameInterval const & b);

/**
 * @brief Convert a TimeFrame-indexed interval to absolute clock ticks.
 * @param interval Interval with start/end as indices into @p time_frame
 * @param time_frame TimeFrame used to resolve indices to ClockTicks
 * @return ClockTicksInterval in physical time
 */
[[nodiscard]] inline ClockTicksInterval toClockTicksInterval(
        TimeFrameInterval const & interval,
        TimeFrame const & time_frame) {
    return ClockTicksInterval{
            time_frame.getTimeAtIndex(interval.start),
            time_frame.getTimeAtIndex(interval.end)};
}

/**
 * @brief Convert a clock-tick interval to TimeFrame indices.
 * @param interval Interval in absolute clock ticks
 * @param time_frame TimeFrame used to resolve ticks to indices
 * @return TimeFrameInterval with start/end indices into @p time_frame
 */
[[nodiscard]] inline TimeFrameInterval toTimeFrameInterval(
        ClockTicksInterval const & interval,
        TimeFrame const & time_frame) {
    return TimeFrameInterval{
            time_frame.getIndexAtTime(interval.start),
            time_frame.getIndexAtTime(interval.end)};
}

#endif//INTERVAL_DATA_HPP
