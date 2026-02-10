#ifndef VERTICAL_AXIS_STATE_HPP
#define VERTICAL_AXIS_STATE_HPP

/**
 * @file VerticalAxisState.hpp
 * @brief Concrete state class for vertical axis functionality
 * 
 * VerticalAxisState is a concrete implementation that can be composed
 * into plot state classes. It manages vertical axis range settings and emits
 * signals when values change.
 */

#include "VerticalAxisStateData.hpp"

#include <QObject>

/**
 * @brief Concrete state class for vertical axis
 * 
 * This class can be used as a member variable in plot state classes
 * to provide vertical axis functionality. It manages the axis data
 * and emits signals when properties change.
 */
class VerticalAxisState : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a VerticalAxisState
     * @param parent Parent QObject
     */
    explicit VerticalAxisState(QObject * parent = nullptr);

    ~VerticalAxisState() override = default;

    // === Y-Axis Range ===

    /**
     * @brief Get the Y-axis minimum value
     * @return Minimum Y value
     */
    [[nodiscard]] double getYMin() const;

    /**
     * @brief Set the Y-axis minimum value
     * @param y_min Minimum Y value
     */
    void setYMin(double y_min);

    /**
     * @brief Get the Y-axis maximum value
     * @return Maximum Y value
     */
    [[nodiscard]] double getYMax() const;

    /**
     * @brief Set the Y-axis maximum value
     * @param y_max Maximum Y value
     */
    void setYMax(double y_max);

    /**
     * @brief Set both Y-axis range values programmatically
     * @param y_min Minimum Y value
     * @param y_max Maximum Y value
     */
    void setRange(double y_min, double y_max);

    /**
     * @brief Set both Y-axis range values programmatically without emitting signals
     * 
     * This is used when updating from external sources (e.g., deserialization)
     * to avoid triggering UI updates. The rangeUpdated signal is still emitted
     * to notify widgets that need to refresh their display.
     * 
     * @param y_min Minimum Y value
     * @param y_max Maximum Y value
     */
    void setRangeSilent(double y_min, double y_max);

    // === Data Access ===

    /**
     * @brief Get the vertical axis data
     * @return Reference to the vertical axis data
     */
    [[nodiscard]] VerticalAxisStateData const & data() const { return _data; }

    /**
     * @brief Get mutable vertical axis data
     * @return Reference to the vertical axis data
     */
    [[nodiscard]] VerticalAxisStateData & data() { return _data; }

signals:
    /**
     * @brief Emitted when Y-axis minimum changes
     * @param y_min New Y-axis minimum value
     */
    void yMinChanged(double y_min);

    /**
     * @brief Emitted when Y-axis maximum changes
     * @param y_max New Y-axis maximum value
     */
    void yMaxChanged(double y_max);

    /**
     * @brief Emitted when Y-axis range changes (either min or max)
     * @param y_min New Y-axis minimum value
     * @param y_max New Y-axis maximum value
     */
    void rangeChanged(double y_min, double y_max);

    /**
     * @brief Emitted when Y-axis range is updated programmatically (e.g., from external source)
     * @param y_min New Y-axis minimum value
     * @param y_max New Y-axis maximum value
     */
    void rangeUpdated(double y_min, double y_max);

private:
    VerticalAxisStateData _data;
};

#endif// VERTICAL_AXIS_STATE_HPP
