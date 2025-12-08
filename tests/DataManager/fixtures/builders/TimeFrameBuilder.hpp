#ifndef TIMEFRAME_BUILDER_HPP
#define TIMEFRAME_BUILDER_HPP

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <vector>
#include <memory>
#include <numeric>

/**
 * @brief Lightweight builder for TimeFrame objects
 * 
 * Provides fluent API for constructing TimeFrame test data without
 * requiring DataManager or other heavy dependencies.
 * 
 * @example Basic usage
 * @code
 * auto tf = TimeFrameBuilder()
 *     .withTimes({0, 10, 20, 30})
 *     .build();
 * @endcode
 * 
 * @example Range-based construction
 * @code
 * auto tf = TimeFrameBuilder()
 *     .withRange(0, 100, 10)  // 0, 10, 20, ..., 100
 *     .build();
 * @endcode
 */
class TimeFrameBuilder {
public:
    TimeFrameBuilder() = default;

    /**
     * @brief Specify time points explicitly
     * @param times Vector of time points in arbitrary units
     */
    TimeFrameBuilder& withTimes(std::vector<int> times) {
        m_times = std::move(times);
        return *this;
    }

    /**
     * @brief Create evenly-spaced time points
     * @param start Starting time point (inclusive)
     * @param end Ending time point (inclusive)
     * @param step Step size between points
     * 
     * @example
     * @code
     * // Creates {0, 10, 20, 30, ..., 100}
     * withRange(0, 100, 10);
     * @endcode
     */
    TimeFrameBuilder& withRange(int start, int end, int step = 1) {
        m_times.clear();
        for (int t = start; t <= end; t += step) {
            m_times.push_back(t);
        }
        return *this;
    }

    /**
     * @brief Create sequential time points starting from 0
     * @param count Number of time points
     * 
     * @example
     * @code
     * // Creates {0, 1, 2, 3, 4}
     * withCount(5);
     * @endcode
     */
    TimeFrameBuilder& withCount(size_t count) {
        m_times.resize(count);
        std::iota(m_times.begin(), m_times.end(), 0);
        return *this;
    }

    /**
     * @brief Create sequential time points with custom start
     * @param count Number of time points
     * @param start Starting value
     * 
     * @example
     * @code
     * // Creates {100, 101, 102, 103, 104}
     * withCountFrom(5, 100);
     * @endcode
     */
    TimeFrameBuilder& withCountFrom(size_t count, int start) {
        m_times.resize(count);
        std::iota(m_times.begin(), m_times.end(), start);
        return *this;
    }

    /**
     * @brief Build the TimeFrame
     * @return Shared pointer to constructed TimeFrame
     */
    std::shared_ptr<TimeFrame> build() const {
        return std::make_shared<TimeFrame>(m_times);
    }

    /**
     * @brief Get the time values (for inspection)
     */
    const std::vector<int>& getTimes() const {
        return m_times;
    }

private:
    std::vector<int> m_times;
};

#endif // TIMEFRAME_BUILDER_HPP
