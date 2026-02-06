#ifndef VERTICAL_AXIS_WITH_RANGE_CONTROLS_HPP
#define VERTICAL_AXIS_WITH_RANGE_CONTROLS_HPP

/**
 * @file VerticalAxisWithRangeControls.hpp
 * @brief Combined widget factory for VerticalAxisWidget with editable range controls
 * 
 * This provides a self-contained widget system that combines:
 * - VerticalAxisWidget: displays the vertical axis with tick marks
 * - Range control spinboxes: editable min/max range inputs
 * 
 * The factory properly links them together with shared state and handles
 * anti-recursion to prevent update loops.
 */

#include "VerticalAxisWidget.hpp"

#include <QWidget>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

#include <memory>

/**
 * @brief Shared state for coordinating between axis widget and range controls
 * 
 * This state object prevents recursive updates by tracking when updates
 * are being applied programmatically vs. user-initiated.
 */
class VerticalAxisRangeState : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a new state object
     * @param parent Parent QObject
     */
    explicit VerticalAxisRangeState(QObject * parent = nullptr);

    /**
     * @brief Get the current minimum range value
     * @return Minimum range value
     */
    [[nodiscard]] double minRange() const { return _min_range; }

    /**
     * @brief Get the current maximum range value
     * @return Maximum range value
     */
    [[nodiscard]] double maxRange() const { return _max_range; }

    /**
     * @brief Set the range values programmatically (without triggering user signals)
     * @param min_range Minimum range value
     * @param max_range Maximum range value
     */
    void setRange(double min_range, double max_range);

    /**
     * @brief Set the range values from user input (triggers rangeChanged signal)
     * @param min_range Minimum range value
     * @param max_range Maximum range value
     */
    void setRangeFromUser(double min_range, double max_range);

signals:
    /**
     * @brief Emitted when the range changes from user input
     * @param min_range New minimum range value
     * @param max_range New maximum range value
     */
    void rangeChanged(double min_range, double max_range);

    /**
     * @brief Emitted when the range is updated programmatically
     * @param min_range New minimum range value
     * @param max_range New maximum range value
     */
    void rangeUpdated(double min_range, double max_range);

private:
    double _min_range = 0.0;
    double _max_range = 100.0;
    bool _updating = false;
};

/**
 * @brief Widget containing spinboxes for editing min/max range
 * 
 * This widget can be placed separately from the axis widget (e.g., in a properties panel).
 * It automatically stays synchronized with the shared state.
 */
class VerticalAxisRangeControls : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct range controls widget
     * @param state Shared state object
     * @param parent Parent widget
     */
    explicit VerticalAxisRangeControls(
        std::shared_ptr<VerticalAxisRangeState> state,
        QWidget * parent = nullptr);

    ~VerticalAxisRangeControls() override = default;

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

private:
    std::shared_ptr<VerticalAxisRangeState> _state;
    QDoubleSpinBox * _min_spinbox;
    QDoubleSpinBox * _max_spinbox;
    bool _updating_ui = false;

    /**
     * @brief Update spinbox values from state
     */
    void updateSpinBoxes();
};

/**
 * @brief Factory result containing all widgets and shared state
 */
struct VerticalAxisWithRangeControls {
    /// Shared state object
    std::shared_ptr<VerticalAxisRangeState> state;

    /// Axis widget (for display in the plot view)
    VerticalAxisWidget * axis_widget;

    /// Range controls widget (can be placed in properties panel)
    VerticalAxisRangeControls * range_controls;

    /**
     * @brief Set the RangeGetter for the axis widget
     * @param getter Function that returns the current range
     */
    void setRangeGetter(VerticalAxisWidget::RangeGetter getter);

    /**
     * @brief Connect axis widget to range changes
     * @tparam SenderType Type of the sender object
     * @param sender Object that emits the signal
     * @param signal Pointer to the signal member function
     */
    template <typename SenderType>
    void connectToRangeChanged(SenderType * sender, void (SenderType::*signal)())
    {
        if (axis_widget && sender) {
            axis_widget->connectToRangeChanged(sender, signal);
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
 * @brief Factory function to create a complete vertical axis with range controls
 * 
 * This factory creates:
 * - A shared state object
 * - A VerticalAxisWidget for display
 * - A VerticalAxisRangeControls widget for editing
 * 
 * All components are properly linked together with anti-recursion handling.
 * 
 * @param axis_parent Parent widget for the axis widget (typically the plot widget)
 * @param controls_parent Parent widget for the range controls (typically properties widget)
 * @return Factory result with all widgets and shared state
 */
VerticalAxisWithRangeControls createVerticalAxisWithRangeControls(
    QWidget * axis_parent = nullptr,
    QWidget * controls_parent = nullptr);

#endif  // VERTICAL_AXIS_WITH_RANGE_CONTROLS_HPP
