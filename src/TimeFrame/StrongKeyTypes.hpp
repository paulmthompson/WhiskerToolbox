
#ifndef STRONG_KEY_TYPES_HPP
#define STRONG_KEY_TYPES_HPP

#include <string>

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