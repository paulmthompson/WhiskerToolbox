#ifndef INTERVAL_DATA_HPP
#define INTERVAL_DATA_HPP

#include <cstdint>

struct Interval {
    int64_t start;
    int64_t end;
};

inline bool operator<(const Interval& a, const Interval& b) {
    return a.start < b.start;
}

inline bool is_overlapping(const Interval& a, const Interval& b) {
    return a.start <= b.end && b.start <= a.end;
}

inline bool is_contiguous(const Interval& a, const Interval& b) {
    return a.end + 1 == b.start || b.end + 1 == a.start;
}

inline bool is_contained(const Interval& a, const Interval& b) {
    return a.start <= b.start && a.end >= b.end;
}

inline bool is_contained(const Interval& a, int64_t time) {
    return a.start <= time && time <= a.end;
}


#endif //INTERVAL_DATA_HPP
