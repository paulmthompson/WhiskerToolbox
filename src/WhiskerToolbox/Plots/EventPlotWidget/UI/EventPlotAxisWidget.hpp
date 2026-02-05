#ifndef EVENT_PLOT_AXIS_WIDGET_HPP
#define EVENT_PLOT_AXIS_WIDGET_HPP

/**
 * @file EventPlotAxisWidget.hpp
 * @brief Widget for rendering the time axis below the event plot
 * 
 * This widget displays a horizontal time axis with tick marks and labels
 * showing the +/- extent relative to the alignment point (t=0).
 */

#include <QWidget>

#include <memory>

class EventPlotState;

/**
 * @brief Widget that renders a horizontal time axis for event plots
 * 
 * Shows:
 * - Time range (e.g., -500 to +500 ms)
 * - Tick marks at regular intervals
 * - Center line at t=0 highlighted
 * - Zoom/pan aware positioning
 */
class EventPlotAxisWidget : public QWidget {
    Q_OBJECT

public:
    explicit EventPlotAxisWidget(QWidget * parent = nullptr);
    ~EventPlotAxisWidget() override = default;

    /**
     * @brief Set the state to use for axis bounds
     * @param state Shared pointer to the EventPlotState
     */
    void setState(std::shared_ptr<EventPlotState> state);

    /**
     * @brief Suggested height for the axis widget
     */
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent * event) override;

private:
    std::shared_ptr<EventPlotState> _state;

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
     * @param time Time value in view coordinates
     * @return Pixel X position
     */
    [[nodiscard]] int timeToPixelX(double time) const;
};

#endif // EVENT_PLOT_AXIS_WIDGET_HPP
