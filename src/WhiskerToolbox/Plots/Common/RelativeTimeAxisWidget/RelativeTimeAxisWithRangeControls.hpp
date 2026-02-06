#ifndef RELATIVE_TIME_AXIS_WITH_RANGE_CONTROLS_HPP
#define RELATIVE_TIME_AXIS_WITH_RANGE_CONTROLS_HPP

/**
 * @file RelativeTimeAxisWithRangeControls.hpp
 * @brief Combined widget factory for RelativeTimeAxisWidget with editable range controls
 * 
 * This provides a self-contained widget system that combines:
 * - RelativeTimeAxisWidget: displays the time axis with tick marks
 * - Range control combo boxes: editable min/max range inputs
 * 
 * The factory properly links them together with shared state and handles
 * anti-recursion to prevent update loops.
 */

#include "RelativeTimeAxisWidget.hpp"

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

#include <memory>

/**
 * @brief Shared state for coordinating between axis widget and range controls
 * 
 * This state object prevents recursive updates by tracking when updates
 * are being applied programmatically vs. user-initiated.
 */
class RelativeTimeAxisRangeState : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a new state object
     * @param parent Parent QObject
     */
    explicit RelativeTimeAxisRangeState(QObject * parent = nullptr);

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
    double _min_range = -30000.0;
    double _max_range = 30000.0;
    bool _updating = false;
};

/**
 * @brief Widget containing combo boxes for editing min/max range
 * 
 * This widget can be placed separately from the axis widget (e.g., in a properties panel).
 * It automatically stays synchronized with the shared state.
 */
class RelativeTimeAxisRangeControls : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct range controls widget
     * @param state Shared state object
     * @param parent Parent widget
     */
    explicit RelativeTimeAxisRangeControls(
        std::shared_ptr<RelativeTimeAxisRangeState> state,
        QWidget * parent = nullptr);

    ~RelativeTimeAxisRangeControls() override = default;

    /**
     * @brief Get the minimum range combo box
     * @return Pointer to the min range combo box
     */
    [[nodiscard]] QComboBox * minRangeCombo() const { return _min_combo; }

    /**
     * @brief Get the maximum range combo box
     * @return Pointer to the max range combo box
     */
    [[nodiscard]] QComboBox * maxRangeCombo() const { return _max_combo; }

private slots:
    /**
     * @brief Handle min range combo box value change
     */
    void onMinRangeChanged();

    /**
     * @brief Handle max range combo box value change
     */
    void onMaxRangeChanged();

    /**
     * @brief Handle state range update (programmatic change)
     * @param min_range New minimum range
     * @param max_range New maximum range
     */
    void onStateRangeUpdated(double min_range, double max_range);

private:
    std::shared_ptr<RelativeTimeAxisRangeState> _state;
    QComboBox * _min_combo;
    QComboBox * _max_combo;
    bool _updating_ui = false;

    /**
     * @brief Update combo box values from state
     */
    void updateComboBoxes();
};

/**
 * @brief Factory result containing all widgets and shared state
 */
struct RelativeTimeAxisWithRangeControls {
    /// Shared state object
    std::shared_ptr<RelativeTimeAxisRangeState> state;

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
 * - A shared state object
 * - A RelativeTimeAxisWidget for display
 * - A RelativeTimeAxisRangeControls widget for editing
 * 
 * All components are properly linked together with anti-recursion handling.
 * 
 * @param axis_parent Parent widget for the axis widget (typically the plot widget)
 * @param controls_parent Parent widget for the range controls (typically properties widget)
 * @return Factory result with all widgets and shared state
 */
RelativeTimeAxisWithRangeControls createRelativeTimeAxisWithRangeControls(
    QWidget * axis_parent = nullptr,
    QWidget * controls_parent = nullptr);

#endif  // RELATIVE_TIME_AXIS_WITH_RANGE_CONTROLS_HPP
