#ifndef RELATIVE_TIME_AXIS_WIDGET_HPP
#define RELATIVE_TIME_AXIS_WIDGET_HPP

/**
 * @file RelativeTimeAxisWidget.hpp
 * @brief Widget for rendering a relative time axis (centered at zero)
 * 
 * This widget displays a horizontal time axis with tick marks and labels
 * showing the +/- extent relative to the alignment point (t=0).
 * It works with CorePlotting::ViewState for generic use across multiple plot types.
 */

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include <QWidget>

#include <functional>
#include <memory>

namespace CorePlotting {
struct ViewState;
}

/**
 * @brief Widget that renders a horizontal time axis for relative time plots
 * 
 * Shows:
 * - Time range (e.g., -500 to +500 ms)
 * - Tick marks at regular intervals
 * - Center line at t=0 highlighted
 * - Zoom/pan aware positioning
 * 
 * This widget is designed to work with CorePlotting::ViewState, making it
 * reusable across EventPlotWidget, HeatmapWidget, PSTHWidget, and other
 * relative time plots.
 */
class RelativeTimeAxisWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Type alias for a function that returns the current ViewState
     */
    using ViewStateGetter = std::function<CorePlotting::ViewState()>;

    /**
     * @brief Construct a RelativeTimeAxisWidget
     * @param parent Parent widget
     */
    explicit RelativeTimeAxisWidget(QWidget * parent = nullptr);

    ~RelativeTimeAxisWidget() override = default;

    /**
     * @brief Set the function to get the current ViewState
     * @param getter Function that returns the current CorePlotting::ViewState
     */
    void setViewStateGetter(ViewStateGetter getter);

    /**
     * @brief Connect to a QObject signal that indicates view state changes
     * 
     * When the signal is emitted, the widget will call the ViewStateGetter
     * to get the updated state and repaint.
     * 
     * @tparam SenderType Type of the sender object
     * @param sender Object that emits the signal
     * @param signal Pointer to the signal member function
     */
    template <typename SenderType>
    void connectToViewStateChanged(SenderType * sender, void (SenderType::*signal)())
    {
        if (sender) {
            connect(sender, signal, this, [this]() { update(); });
        }
    }

    /**
     * @brief Suggested height for the axis widget
     */
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent * event) override;

private:
    ViewStateGetter _view_state_getter;

    // Axis styling constants
    static constexpr int kAxisHeight = 30;
    static constexpr int kTickHeight = 5;
    static constexpr int kMajorTickHeight = 8;
    static constexpr int kLabelOffset = 12;

    /**
     * @brief Compute nice tick intervals for the current range
     * @param range Total range to display
     * @return Tick interval value
     */
    [[nodiscard]] double computeTickInterval(double range) const;

    /**
     * @brief Convert time value to pixel X position
     * @param time Time value in world coordinates
     * @param view_state Current view state
     * @return Pixel X position
     */
    [[nodiscard]] int timeToPixelX(double time, CorePlotting::ViewState const & view_state) const;
};

#endif  // RELATIVE_TIME_AXIS_WIDGET_HPP
