#ifndef RELATIVE_TIME_AXIS_STATE_HPP
#define RELATIVE_TIME_AXIS_STATE_HPP

/**
 * @file RelativeTimeAxisState.hpp
 * @brief Concrete state class for relative time axis functionality
 * 
 * RelativeTimeAxisState is a concrete implementation that can be composed
 * into plot state classes. It manages relative time axis range settings and emits
 * signals when values change.
 */

#include "RelativeTimeAxisStateData.hpp"

#include <QObject>

/**
 * @brief Concrete state class for relative time axis
 * 
 * This class can be used as a member variable in plot state classes
 * to provide relative time axis functionality. It manages the axis data
 * and emits signals when properties change.
 */
class RelativeTimeAxisState : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a RelativeTimeAxisState
     * @param parent Parent QObject
     */
    explicit RelativeTimeAxisState(QObject * parent = nullptr);

    ~RelativeTimeAxisState() override = default;

    // === Time Axis Range ===

    /**
     * @brief Get the minimum time range value
     * @return Minimum time range value
     */
    [[nodiscard]] double getMinRange() const;

    /**
     * @brief Set the minimum time range value
     * @param min_range Minimum time range value
     */
    void setMinRange(double min_range);

    /**
     * @brief Get the maximum time range value
     * @return Maximum time range value
     */
    [[nodiscard]] double getMaxRange() const;

    /**
     * @brief Set the maximum time range value
     * @param max_range Maximum time range value
     */
    void setMaxRange(double max_range);

    /**
     * @brief Set both time range values programmatically
     * @param min_range Minimum time range value
     * @param max_range Maximum time range value
     */
    void setRange(double min_range, double max_range);

    /**
     * @brief Set both time range values programmatically without emitting signals
     * 
     * This is used when updating from external sources (e.g., deserialization)
     * to avoid triggering UI updates. The rangeUpdated signal is still emitted
     * to notify widgets that need to refresh their display.
     * 
     * @param min_range Minimum time range value
     * @param max_range Maximum time range value
     */
    void setRangeSilent(double min_range, double max_range);

    // === Data Access ===

    /**
     * @brief Get the relative time axis data
     * @return Reference to the relative time axis data
     */
    [[nodiscard]] RelativeTimeAxisStateData const & data() const { return _data; }

    /**
     * @brief Get mutable relative time axis data
     * @return Reference to the relative time axis data
     */
    [[nodiscard]] RelativeTimeAxisStateData & data() { return _data; }

signals:
    /**
     * @brief Emitted when minimum time range changes
     * @param min_range New minimum time range value
     */
    void minRangeChanged(double min_range);

    /**
     * @brief Emitted when maximum time range changes
     * @param max_range New maximum time range value
     */
    void maxRangeChanged(double max_range);

    /**
     * @brief Emitted when time range changes (either min or max)
     * @param min_range New minimum time range value
     * @param max_range New maximum time range value
     */
    void rangeChanged(double min_range, double max_range);

    /**
     * @brief Emitted when time range is updated programmatically (e.g., from external source)
     * @param min_range New minimum time range value
     * @param max_range New maximum time range value
     */
    void rangeUpdated(double min_range, double max_range);

private:
    RelativeTimeAxisStateData _data;
};

#endif// RELATIVE_TIME_AXIS_STATE_HPP
