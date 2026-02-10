#ifndef RELATIVE_TIME_AXIS_WITH_RANGE_CONTROLS_HPP
#define RELATIVE_TIME_AXIS_WITH_RANGE_CONTROLS_HPP

/**
 * @file RelativeTimeAxisWithRangeControls.hpp
 * @brief Combined widget factory for RelativeTimeAxisWidget with editable range controls
 * 
 * This provides a self-contained widget system that combines:
 * - RelativeTimeAxisWidget: displays the time axis with tick marks
 * - Range control spinboxes: editable min/max range inputs
 * 
 * The factory properly links them together with shared state and handles
 * anti-recursion to prevent update loops.
 */

#include "RelativeTimeAxisWidget.hpp"
#include "Core/RelativeTimeAxisState.hpp"

#include <QWidget>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

/**
 * @brief Widget containing spinboxes for editing min/max range
 * 
 * This widget can be placed separately from the axis widget (e.g., in a properties panel).
 * It automatically stays synchronized with the RelativeTimeAxisState.
 */
class RelativeTimeAxisRangeControls : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct range controls widget
     * @param state RelativeTimeAxisState object (must outlive this widget)
     * @param parent Parent widget
     */
    explicit RelativeTimeAxisRangeControls(
        RelativeTimeAxisState * state,
        QWidget * parent = nullptr);

    ~RelativeTimeAxisRangeControls() override = default;

    /**
     * @brief Get the minimum range spinbox
     * @return Pointer to the min range spinbox
     */
    [[nodiscard]] QDoubleSpinBox * minRangeSpinBox() const { return _min_spinbox; }

    /**
     * @brief Get the maximum range spinbox
     * @return Pointer to the max range spinbox
     */
    [[nodiscard]] QDoubleSpinBox * maxRangeSpinBox() const { return _max_spinbox; }

private slots:
    /**
     * @brief Handle min range spinbox value change
     */
    void onMinRangeChanged(double value);

    /**
     * @brief Handle max range spinbox value change
     */
    void onMaxRangeChanged(double value);

    /**
     * @brief Handle state range update (programmatic change)
     * @param min_range New minimum range
     * @param max_range New maximum range
     */
    void onStateRangeUpdated(double min_range, double max_range);

    /**
     * @brief Handle state range change (user or programmatic)
     * @param min_range New minimum range
     * @param max_range New maximum range
     */
    void onStateRangeChanged(double min_range, double max_range);

private:
    RelativeTimeAxisState * _state;
    QDoubleSpinBox * _min_spinbox;
    QDoubleSpinBox * _max_spinbox;
    bool _updating_ui = false;

    /**
     * @brief Update spinbox values from state
     */
    void updateSpinBoxes();
};

/**
 * @brief Factory result containing all widgets linked to RelativeTimeAxisState
 */
struct RelativeTimeAxisWithRangeControls {
    /// RelativeTimeAxisState object (owned by PSTHState or similar)
    RelativeTimeAxisState * state;

    /// Axis widget (for display in the plot view)
    RelativeTimeAxisWidget * axis_widget;

    /// Range controls widget (can be placed in properties panel)
    RelativeTimeAxisRangeControls * range_controls;

    /**
     * @brief Set the ViewState getter for the axis widget
     * @param getter Function that returns the current ViewState
     */
    void setViewStateGetter(RelativeTimeAxisWidget::ViewStateGetter getter);

    /**
     * @brief Connect axis widget to view state changes
     * @tparam SenderType Type of the sender object
     * @param sender Object that emits the signal
     * @param signal Pointer to the signal member function
     */
    template <typename SenderType>
    void connectToViewStateChanged(SenderType * sender, void (SenderType::*signal)())
    {
        if (axis_widget && sender) {
            axis_widget->connectToViewStateChanged(sender, signal);
        }
    }

    /**
     * @brief Set the range values programmatically
     * @param min_range Minimum range value
     * @param max_range Maximum range value
     */
    void setRange(double min_range, double max_range);

    /**
     * @brief Get the current range values
     * @param min_range Output parameter for minimum range
     * @param max_range Output parameter for maximum range
     */
    void getRange(double & min_range, double & max_range) const;
};

/**
 * @brief Factory function to create a complete relative time axis with range controls
 * 
 * This factory creates:
 * - A RelativeTimeAxisWidget for display
 * - A RelativeTimeAxisRangeControls widget for editing
 * 
 * All components are linked to the provided RelativeTimeAxisState and handle
 * anti-recursion to prevent update loops.
 * 
 * @param state RelativeTimeAxisState object (must outlive the returned widgets)
 * @param axis_parent Parent widget for the axis widget (typically the plot widget)
 * @param controls_parent Parent widget for the range controls (typically properties widget)
 * @return Factory result with all widgets linked to the state
 */
RelativeTimeAxisWithRangeControls createRelativeTimeAxisWithRangeControls(
    RelativeTimeAxisState * state,
    QWidget * axis_parent = nullptr,
    QWidget * controls_parent = nullptr);

#endif  // RELATIVE_TIME_AXIS_WITH_RANGE_CONTROLS_HPP
