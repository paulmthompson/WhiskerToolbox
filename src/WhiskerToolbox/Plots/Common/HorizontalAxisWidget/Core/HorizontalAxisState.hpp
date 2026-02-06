#ifndef HORIZONTAL_AXIS_STATE_HPP
#define HORIZONTAL_AXIS_STATE_HPP

/**
 * @file HorizontalAxisState.hpp
 * @brief Concrete state class for horizontal axis functionality
 * 
 * HorizontalAxisState is a concrete implementation that can be composed
 * into plot state classes. It manages horizontal axis range settings and emits
 * signals when values change.
 */

#include "HorizontalAxisStateData.hpp"

#include <QObject>

/**
 * @brief Concrete state class for horizontal axis
 * 
 * This class can be used as a member variable in plot state classes
 * to provide horizontal axis functionality. It manages the axis data
 * and emits signals when properties change.
 */
class HorizontalAxisState : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a HorizontalAxisState
     * @param parent Parent QObject
     */
    explicit HorizontalAxisState(QObject * parent = nullptr);

    ~HorizontalAxisState() override = default;

    // === X-Axis Range ===

    /**
     * @brief Get the X-axis minimum value
     * @return Minimum X value
     */
    [[nodiscard]] double getXMin() const;

    /**
     * @brief Set the X-axis minimum value
     * @param x_min Minimum X value
     */
    void setXMin(double x_min);

    /**
     * @brief Get the X-axis maximum value
     * @return Maximum X value
     */
    [[nodiscard]] double getXMax() const;

    /**
     * @brief Set the X-axis maximum value
     * @param x_max Maximum X value
     */
    void setXMax(double x_max);

    /**
     * @brief Set both X-axis range values programmatically
     * @param x_min Minimum X value
     * @param x_max Maximum X value
     */
    void setRange(double x_min, double x_max);

    /**
     * @brief Set both X-axis range values programmatically without emitting signals
     * 
     * This is used when updating from external sources (e.g., deserialization)
     * to avoid triggering UI updates. The rangeUpdated signal is still emitted
     * to notify widgets that need to refresh their display.
     * 
     * @param x_min Minimum X value
     * @param x_max Maximum X value
     */
    void setRangeSilent(double x_min, double x_max);

    // === Data Access ===

    /**
     * @brief Get the horizontal axis data
     * @return Reference to the horizontal axis data
     */
    [[nodiscard]] HorizontalAxisStateData const & data() const { return _data; }

    /**
     * @brief Get mutable horizontal axis data
     * @return Reference to the horizontal axis data
     */
    [[nodiscard]] HorizontalAxisStateData & data() { return _data; }

signals:
    /**
     * @brief Emitted when X-axis minimum changes
     * @param x_min New X-axis minimum value
     */
    void xMinChanged(double x_min);

    /**
     * @brief Emitted when X-axis maximum changes
     * @param x_max New X-axis maximum value
     */
    void xMaxChanged(double x_max);

    /**
     * @brief Emitted when X-axis range changes (either min or max)
     * @param x_min New X-axis minimum value
     * @param x_max New X-axis maximum value
     */
    void rangeChanged(double x_min, double x_max);

    /**
     * @brief Emitted when X-axis range is updated programmatically (e.g., from external source)
     * @param x_min New X-axis minimum value
     * @param x_max New X-axis maximum value
     */
    void rangeUpdated(double x_min, double x_max);

private:
    HorizontalAxisStateData _data;
};

#endif// HORIZONTAL_AXIS_STATE_HPP
