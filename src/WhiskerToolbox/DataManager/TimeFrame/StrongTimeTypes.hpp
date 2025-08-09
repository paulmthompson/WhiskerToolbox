#ifndef STRONG_TIME_TYPES_HPP
#define STRONG_TIME_TYPES_HPP

#include <cstdint>
#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

/**
 * @brief Strong type for camera frame indices
 * 
 * Represents indices into camera frame sequences. Cannot be directly
 * converted to other time coordinate systems without explicit conversion
 * through a TimeFrame.
 */
class CameraFrameIndex {
public:
    explicit CameraFrameIndex(int64_t value) : _value(value) {}
    
    [[nodiscard]] int64_t getValue() const { return _value; }
    
    // Comparison operators
    bool operator==(CameraFrameIndex const& other) const { return _value == other._value; }
    bool operator!=(CameraFrameIndex const& other) const { return _value != other._value; }
    bool operator<(CameraFrameIndex const& other) const { return _value < other._value; }
    bool operator<=(CameraFrameIndex const& other) const { return _value <= other._value; }
    bool operator>(CameraFrameIndex const& other) const { return _value > other._value; }
    bool operator>=(CameraFrameIndex const& other) const { return _value >= other._value; }
    
    // Arithmetic operations
    CameraFrameIndex operator+(int64_t offset) const { return CameraFrameIndex(_value + offset); }
    CameraFrameIndex operator-(int64_t offset) const { return CameraFrameIndex(_value - offset); }
    int64_t operator-(CameraFrameIndex const& other) const { return _value - other._value; }
    
private:
    int64_t _value;
};

/**
 * @brief Strong type for clock tick values
 * 
 * Represents raw clock ticks from acquisition hardware. Can be converted
 * to seconds if the sampling rate is known (stored in associated TimeFrame).
 * Multiple data streams using the same clock can be confidently synchronized.
 */
class ClockTicks {
public:
    explicit ClockTicks(int64_t value) : _value(value) {}
    
    [[nodiscard]] int64_t getValue() const { return _value; }
    
    // Comparison operators
    bool operator==(ClockTicks const& other) const { return _value == other._value; }
    bool operator!=(ClockTicks const& other) const { return _value != other._value; }
    bool operator<(ClockTicks const& other) const { return _value < other._value; }
    bool operator<=(ClockTicks const& other) const { return _value <= other._value; }
    bool operator>(ClockTicks const& other) const { return _value > other._value; }
    bool operator>=(ClockTicks const& other) const { return _value >= other._value; }
    
    // Arithmetic operations
    ClockTicks operator+(int64_t offset) const { return ClockTicks(_value + offset); }
    ClockTicks operator-(int64_t offset) const { return ClockTicks(_value - offset); }
    int64_t operator-(ClockTicks const& other) const { return _value - other._value; }
    
private:
    int64_t _value;
};

/**
 * @brief Strong type for time values in seconds
 * 
 * Represents absolute time in seconds. Can be converted to/from other
 * time coordinate systems when calibration information is available.
 */
class Seconds {
public:
    explicit Seconds(double value) : _value(value) {}
    
    [[nodiscard]] double getValue() const { return _value; }
    
    // Comparison operators
    bool operator==(Seconds const& other) const { return _value == other._value; }
    bool operator!=(Seconds const& other) const { return _value != other._value; }
    bool operator<(Seconds const& other) const { return _value < other._value; }
    bool operator<=(Seconds const& other) const { return _value <= other._value; }
    bool operator>(Seconds const& other) const { return _value > other._value; }
    bool operator>=(Seconds const& other) const { return _value >= other._value; }
    
    // Arithmetic operations
    Seconds operator+(double offset) const { return Seconds(_value + offset); }
    Seconds operator-(double offset) const { return Seconds(_value - offset); }
    double operator-(Seconds const& other) const { return _value - other._value; }
    
private:
    double _value;
};

/**
 * @brief Strong type for uncalibrated coordinate values
 * 
 * Represents time coordinate values that haven't been calibrated or
 * whose coordinate system is unknown. Requires explicit unsafe casting
 * to convert to other coordinate systems.
 * 
 * Use this type when processing data where the time coordinate system
 * is not important (e.g., algorithmic processing that preserves timing).
 */
class UncalibratedIndex {
public:
    explicit UncalibratedIndex(int64_t value) : _value(value) {}
    
    [[nodiscard]] int64_t getValue() const { return _value; }
    
    // Comparison operators
    bool operator==(UncalibratedIndex const& other) const { return _value == other._value; }
    bool operator!=(UncalibratedIndex const& other) const { return _value != other._value; }
    bool operator<(UncalibratedIndex const& other) const { return _value < other._value; }
    bool operator<=(UncalibratedIndex const& other) const { return _value <= other._value; }
    bool operator>(UncalibratedIndex const& other) const { return _value > other._value; }
    bool operator>=(UncalibratedIndex const& other) const { return _value >= other._value; }
    
    // Arithmetic operations
    UncalibratedIndex operator+(int64_t offset) const { return UncalibratedIndex(_value + offset); }
    UncalibratedIndex operator-(int64_t offset) const { return UncalibratedIndex(_value - offset); }
    int64_t operator-(UncalibratedIndex const& other) const { return _value - other._value; }
    
    /**
     * @brief Unsafe conversion to CameraFrameIndex
     * 
     * WARNING: This conversion assumes that the uncalibrated index
     * represents camera frame indices. Use only when you are certain
     * of the coordinate system.
     * 
     * @return CameraFrameIndex with the same numeric value
     */
    [[nodiscard]] CameraFrameIndex unsafeToCameraFrameIndex() const {
        return CameraFrameIndex(_value);
    }
    
    /**
     * @brief Unsafe conversion to ClockTicks
     * 
     * WARNING: This conversion assumes that the uncalibrated index
     * represents clock tick values. Use only when you are certain
     * of the coordinate system.
     * 
     * @return ClockTicks with the same numeric value
     */
    [[nodiscard]] ClockTicks unsafeToClockTicks() const {
        return ClockTicks(_value);
    }
    
private:
    int64_t _value;
};

/**
 * @brief Strong type for indices into data arrays within time series objects
 * 
 * Represents direct indices into the _data vector of time series classes.
 * This is distinct from TimeFrameIndex (which indexes into time coordinate space)
 * and from time coordinates themselves. Use this when you need to access
 * data by its position in the storage array, regardless of time semantics.
 */
class DataArrayIndex {
public:
    explicit DataArrayIndex(size_t value) : _value(value) {}
    
    [[nodiscard]] size_t getValue() const { return _value; }
    
    // Comparison operators
    bool operator==(DataArrayIndex const& other) const { return _value == other._value; }
    bool operator!=(DataArrayIndex const& other) const { return _value != other._value; }
    bool operator<(DataArrayIndex const& other) const { return _value < other._value; }
    bool operator<=(DataArrayIndex const& other) const { return _value <= other._value; }
    bool operator>(DataArrayIndex const& other) const { return _value > other._value; }
    bool operator>=(DataArrayIndex const& other) const { return _value >= other._value; }
    
    // Arithmetic operations
    DataArrayIndex operator+(size_t offset) const { return DataArrayIndex(_value + offset); }
    DataArrayIndex operator-(size_t offset) const { return DataArrayIndex(_value - offset); }
    size_t operator-(DataArrayIndex const& other) const { return _value - other._value; }
    
    // Pre and post increment/decrement for iteration
    DataArrayIndex& operator++() { ++_value; return *this; }
    DataArrayIndex operator++(int) { DataArrayIndex temp(*this); ++_value; return temp; }
    DataArrayIndex& operator--() { --_value; return *this; }
    DataArrayIndex operator--(int) { DataArrayIndex temp(*this); --_value; return temp; }
    
private:
    size_t _value;
};

/**
 * @brief Variant type that can hold any of the strong time coordinate types
 * 
 * Useful for APIs that need to work with multiple coordinate systems
 * or for storing time coordinates whose type is determined at runtime.
 */
using TimeCoordinate = std::variant<CameraFrameIndex, ClockTicks, Seconds, UncalibratedIndex>;

/**
 * @brief Helper function to extract the numeric value from any TimeCoordinate
 * 
 * @param coord The time coordinate variant
 * @return The underlying numeric value (int64_t for integer types, double for Seconds)
 */
template<typename ReturnType = int64_t>
[[nodiscard]] ReturnType getTimeValue(TimeCoordinate const& coord) {
    return std::visit([](auto const& value) -> ReturnType {
        if constexpr (std::is_same_v<decltype(value), Seconds const&>) {
            if constexpr (std::is_same_v<ReturnType, double>) {
                return value.getValue();
            } else {
                return static_cast<ReturnType>(value.getValue());
            }
        } else {
            if constexpr (std::is_same_v<ReturnType, double>) {
                return static_cast<double>(value.getValue());
            } else {
                return static_cast<ReturnType>(value.getValue());
            }
        }
    }, coord);
}

#endif // STRONG_TIME_TYPES_HPP 

// ======================== Strong key types ========================

#ifndef STRONG_KEY_TYPES_HPP
#define STRONG_KEY_TYPES_HPP

/**
 * @brief Strong type wrapper for data keys used to index data objects in DataManager
 */
class DataKey {
public:
    /**
     * @brief Construct an empty DataKey
     */
    DataKey() = default;

    /**
     * @brief Explicitly construct from std::string
     * @param value Key string
     */
    explicit DataKey(std::string value) : _value(std::move(value)) {}

    /**
     * @brief Explicitly construct from std::string_view
     * @param value Key string view
     */
    explicit DataKey(std::string_view value) : _value(value) {}

    /**
     * @brief Access the underlying string value
     */
    [[nodiscard]] std::string const & str() const noexcept { return _value; }

    /**
     * @brief Check if the key is empty
     */
    [[nodiscard]] bool empty() const noexcept { return _value.empty(); }

    // Comparisons
    [[nodiscard]] bool operator==(DataKey const & other) const noexcept { return _value == other._value; }
    [[nodiscard]] bool operator!=(DataKey const & other) const noexcept { return _value != other._value; }
    [[nodiscard]] bool operator<(DataKey const & other) const noexcept { return _value < other._value; }

private:
    std::string _value;
};

/**
 * @brief Strong type wrapper for time frame keys used to index TimeFrame objects in DataManager
 */
class TimeKey {
public:
    /**
     * @brief Construct an empty TimeKey
     */
    TimeKey() = default;
    // construct from const char*
    explicit TimeKey(const char* value) : _value(value) {}

    /**
     * @brief Explicitly construct from std::string
     * @param value Key string
     */
    explicit TimeKey(std::string value) : _value(std::move(value)) {}

    /**
     * @brief Explicitly construct from std::string_view
     * @param value Key string view
     */
    explicit TimeKey(std::string_view value) : _value(value) {}

    /**
     * @brief Access the underlying string value
     */
    [[nodiscard]] std::string const & str() const noexcept { return _value; }

    /**
     * @brief Check if the key is empty
     */
    [[nodiscard]] bool empty() const noexcept { return _value.empty(); }

    // Comparisons
    [[nodiscard]] bool operator==(TimeKey const & other) const noexcept { return _value == other._value; }
    [[nodiscard]] bool operator!=(TimeKey const & other) const noexcept { return _value != other._value; }
    [[nodiscard]] bool operator<(TimeKey const & other) const noexcept { return _value < other._value; }

private:
    std::string _value;
};

/**
 * @brief Stream insertion for DataKey
 */
inline std::ostream & operator<<(std::ostream & os, DataKey const & key) {
    os << key.str();
    return os;
}

/**
 * @brief Stream insertion for TimeKey
 */
inline std::ostream & operator<<(std::ostream & os, TimeKey const & key) {
    os << key.str();
    return os;
}

/**
 * @brief Convert DataKey to std::string
 */
inline std::string to_string(DataKey const & key) { return key.str(); }

/**
 * @brief Convert TimeKey to std::string
 */
inline std::string to_string(TimeKey const & key) { return key.str(); }

namespace std {
    /**
     * @brief std::hash specialization for DataKey
     */
    template<>
    struct hash<DataKey> {
        size_t operator()(DataKey const & key) const noexcept {
            return std::hash<std::string>{}(key.str());
        }
    };

    /**
     * @brief std::hash specialization for TimeKey
     */
    template<>
    struct hash<TimeKey> {
        size_t operator()(TimeKey const & key) const noexcept {
            return std::hash<std::string>{}(key.str());
        }
    };
}

#endif // STRONG_KEY_TYPES_HPP