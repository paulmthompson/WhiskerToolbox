#ifndef PLOT_ALIGNMENT_STATE_HPP
#define PLOT_ALIGNMENT_STATE_HPP

/**
 * @file PlotAlignmentState.hpp
 * @brief Concrete state class for plot alignment functionality
 * 
 * PlotAlignmentState is a concrete implementation that can be composed
 * into plot state classes. It manages alignment settings and emits
 * signals when values change.
 */

#include "PlotAlignmentData.hpp"

#include <QObject>
#include <QString>

/**
 * @brief Concrete state class for plot alignment
 * 
 * This class can be used as a member variable in plot state classes
 * to provide alignment functionality. It manages the alignment data
 * and emits signals when properties change.
 */
class PlotAlignmentState : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a PlotAlignmentState
     * @param parent Parent QObject
     */
    explicit PlotAlignmentState(QObject * parent = nullptr);

    ~PlotAlignmentState() override = default;

    // === Alignment Event ===

    /**
     * @brief Get the alignment event key
     * @return Key of the selected event/interval series
     */
    [[nodiscard]] QString getAlignmentEventKey() const;

    /**
     * @brief Set the alignment event key
     * @param key Key of the event/interval series to use for alignment
     */
    void setAlignmentEventKey(QString const & key);

    // === Interval Alignment ===

    /**
     * @brief Get the interval alignment type
     * @return Whether to use beginning or end of intervals
     */
    [[nodiscard]] IntervalAlignmentType getIntervalAlignmentType() const;

    /**
     * @brief Set the interval alignment type
     * @param type Whether to use beginning or end of intervals
     */
    void setIntervalAlignmentType(IntervalAlignmentType type);

    // === Offset ===

    /**
     * @brief Get the offset value
     * @return Offset in time units
     */
    [[nodiscard]] double getOffset() const;

    /**
     * @brief Set the offset value
     * @param offset Offset in time units to apply to alignment events
     */
    void setOffset(double offset);

    // === Window Size ===

    /**
     * @brief Get the window size
     * @return Window size in time units
     */
    [[nodiscard]] double getWindowSize() const;

    /**
     * @brief Set the window size
     * @param window_size Window size in time units to gather around alignment event
     */
    void setWindowSize(double window_size);

    // === Data Access ===

    /**
     * @brief Get the alignment data
     * @return Reference to the alignment data
     */
    [[nodiscard]] PlotAlignmentData const & data() const { return _data; }

    /**
     * @brief Get mutable alignment data
     * @return Reference to the alignment data
     */
    [[nodiscard]] PlotAlignmentData & data() { return _data; }

signals:
    /**
     * @brief Emitted when alignment event key changes
     * @param key New alignment event key
     */
    void alignmentEventKeyChanged(QString const & key);

    /**
     * @brief Emitted when interval alignment type changes
     * @param type New interval alignment type
     */
    void intervalAlignmentTypeChanged(IntervalAlignmentType type);

    /**
     * @brief Emitted when offset changes
     * @param offset New offset value
     */
    void offsetChanged(double offset);

    /**
     * @brief Emitted when window size changes
     * @param window_size New window size value
     */
    void windowSizeChanged(double window_size);

private:
    PlotAlignmentData _data;
};

#endif// PLOT_ALIGNMENT_STATE_HPP
