#ifndef INTERVAL_DATA_HPP
#define INTERVAL_DATA_HPP

#include "TimeFrame.hpp"

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



#endif//INTERVAL_DATA_HPP
