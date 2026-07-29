#ifndef NEURALYZER_CLOCK_TICKS_HPP
#define NEURALYZER_CLOCK_TICKS_HPP

#include <cstdint>


/**
 * @brief Strong type for clock tick values
 * 
 * Represents raw clock ticks from acquisition hardware. Can be converted
 * to seconds if the sampling rate is known (stored in associated TimeFrame).
 * Multiple data streams using the same clock can be confidently synchronized.
 */
class ClockTicks {
public:
    explicit ClockTicks(int64_t value)
        : _value(value) {}

    [[nodiscard]] int64_t getValue() const { return _value; }

    // Comparison operators
    bool operator==(ClockTicks const & other) const { return _value == other._value; }
    bool operator!=(ClockTicks const & other) const { return _value != other._value; }
    bool operator<(ClockTicks const & other) const { return _value < other._value; }
    bool operator<=(ClockTicks const & other) const { return _value <= other._value; }
    bool operator>(ClockTicks const & other) const { return _value > other._value; }
    bool operator>=(ClockTicks const & other) const { return _value >= other._value; }

    // Arithmetic operations
    ClockTicks operator+(int64_t offset) const { return ClockTicks(_value + offset); }
    ClockTicks operator-(int64_t offset) const { return ClockTicks(_value - offset); }
    int64_t operator-(ClockTicks const & other) const { return _value - other._value; }

private:
    int64_t _value;
};


#endif// NEURALYZER_CLOCK_TICKS_HPP