#ifndef TIME_FRAME_INDEX_HPP
#define TIME_FRAME_INDEX_HPP

#include <cstdint>

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

[[nodiscard]] constexpr TimeFrameIndex operator*(TimeFrameIndex idx, int64_t scale) {
    return TimeFrameIndex(idx.getValue() * scale);
}
[[nodiscard]] constexpr TimeFrameIndex operator*(int64_t scale, TimeFrameIndex idx) {
    return idx * scale;
}
[[nodiscard]] constexpr TimeFrameIndex operator/(TimeFrameIndex idx, int64_t divisor) {
    return TimeFrameIndex(idx.getValue() / divisor);
}

#endif// TIME_FRAME_INDEX_HPP