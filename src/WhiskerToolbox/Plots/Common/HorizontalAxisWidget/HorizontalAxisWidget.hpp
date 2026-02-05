#ifndef HORIZONTAL_AXIS_WIDGET_HPP
#define HORIZONTAL_AXIS_WIDGET_HPP

/**
 * @file HorizontalAxisWidget.hpp
 * @brief Widget for rendering a horizontal axis with tick marks
 * 
 * This widget displays a horizontal axis with tick marks and labels
 * showing world coordinate values. It can be used for X-axis display
 * in various plot widgets.
 */

#include <QWidget>

#include <functional>
#include <memory>

class QPaintEvent;

/**
 * @brief Widget that renders a horizontal axis for plots
 * 
 * Shows:
 * - Value range (e.g., 0 to 100)
 * - Tick marks at regular intervals
 * - Labels for major ticks
 * - Updates when range changes
 */
class HorizontalAxisWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Type alias for a function that returns the current min/max range
     */
    using RangeGetter = std::function<std::pair<double, double>()>;

    /**
     * @brief Construct a HorizontalAxisWidget
     * @param parent Parent widget
     */
    explicit HorizontalAxisWidget(QWidget * parent = nullptr);

    ~HorizontalAxisWidget() override = default;

    /**
     * @brief Set the function to get the current range
     * @param getter Function that returns (min, max) pair
     */
    void setRangeGetter(RangeGetter getter);

    /**
     * @brief Set the range directly (for simple cases)
     * @param min Minimum value
     * @param max Maximum value
     */
    void setRange(double min, double max);

    /**
     * @brief Connect to a QObject signal that indicates range changes
     * 
     * When the signal is emitted, the widget will call the RangeGetter
     * to get the updated range and repaint.
     * 
     * @tparam SenderType Type of the sender object
     * @param sender Object that emits the signal
     * @param signal Pointer to the signal member function
     */
    template <typename SenderType>
    void connectToRangeChanged(SenderType * sender, void (SenderType::*signal)())
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
    RangeGetter _range_getter;
    double _min_value = 0.0;
    double _max_value = 100.0;
    bool _use_getter = false;

    // Axis styling constants
    static constexpr int kAxisHeight = 50;
    static constexpr int kTickHeight = 5;
    static constexpr int kMajorTickHeight = 8;
    static constexpr int kLabelOffset = 5;

    /**
     * @brief Compute nice tick intervals for the current range
     * @param range Total range to display
     * @return Tick interval value
     */
    [[nodiscard]] double computeTickInterval(double range) const;

    /**
     * @brief Convert world X value to pixel X position
     * @param value World X value
     * @param min Minimum world value
     * @param max Maximum world value
     * @return Pixel X position (0 = left, width = right)
     */
    [[nodiscard]] int valueToPixelX(double value, double min, double max) const;
};

#endif  // HORIZONTAL_AXIS_WIDGET_HPP
