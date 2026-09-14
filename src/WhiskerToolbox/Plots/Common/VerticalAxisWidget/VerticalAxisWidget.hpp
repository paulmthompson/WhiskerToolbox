#ifndef VERTICAL_AXIS_WIDGET_HPP
#define VERTICAL_AXIS_WIDGET_HPP

/**
 * @file VerticalAxisWidget.hpp
 * @brief Widget for rendering a vertical axis with tick marks
 *
 * This widget displays a vertical axis with tick marks and labels
 * showing world coordinate values. It can be used for Y-axis display
 * in various plot widgets and as a left ruler in the Media Viewer.
 */

#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "Common/AxisTickLayout.hpp"

#include <QColor>
#include <QWidget>

#include <functional>
#include <optional>

class QPaintEvent;

/**
 * @brief Widget that renders a vertical axis for plots
 *
 * Shows:
 * - Value range (e.g., 0 to 100)
 * - Tick marks at regular intervals
 * - Labels for major ticks
 * - Updates when range changes
 */
class VerticalAxisWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Type alias for a function that returns the current min/max range
     */
    using RangeGetter = std::function<std::pair<double, double>()>;

    /**
     * @brief Construct a VerticalAxisWidget
     * @param parent Parent widget
     */
    explicit VerticalAxisWidget(QWidget * parent = nullptr);

    ~VerticalAxisWidget() override = default;

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
     * @brief Set an AxisMapping to control label formatting and domain interpretation
     *
     * When set, the RangeGetter/setRange values are interpreted as domain values.
     * The AxisMapping's formatLabel is used for tick labels instead of the default
     * decimal formatting.
     *
     * @param mapping The axis mapping describing world↔domain↔label relationships
     */
    void setAxisMapping(CorePlotting::AxisMapping mapping);

    /**
     * @brief Clear any previously set AxisMapping, reverting to default formatting
     */
    void clearAxisMapping();

    /**
     * @brief Get the current AxisMapping, if any
     * @return Pointer to the mapping, or nullptr if none set
     */
    [[nodiscard]] CorePlotting::AxisMapping const * axisMapping() const;

    /**
     * @brief Set whether the Y axis is inverted (min at top, max at bottom)
     *
     * When inverted, the axis displays like image coordinates where Y=0 is at
     * the top and Y increases downward. This matches the convention used by
     * MediaWidget and is appropriate for spatial data (points, masks, lines)
     * derived from image frames.
     *
     * @param inverted true to invert the Y axis
     */
    void setInverted(bool inverted);

    /**
     * @brief Check whether the Y axis is currently inverted
     * @return true if the axis is inverted (min at top, max at bottom)
     */
    [[nodiscard]] bool isInverted() const;

    /**
     * @brief Set plot or ruler display mode
     * @param mode Display mode
     */
    void setDisplayMode(Neuralyzer::Plots::AxisDisplayMode mode);

    /**
     * @brief Set the widget thickness in pixels
     * @param px Width in pixels
     */
    void setThickness(int px);

    /**
     * @brief Show or hide min/max extent labels
     * @param show Whether to show extent labels
     */
    void setShowExtentLabels(bool show);

    /**
     * @brief Set the color used for negative tick values and labels
     * @param color Label and tick color when value < 0
     */
    void setNegativeLabelColor(QColor color);

    /**
     * @brief Set tick generation configuration
     * @param config Tick spacing configuration
     */
    void setTickConfig(Neuralyzer::Plots::AxisTickConfig config);

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
     * @brief Suggested width for the axis widget
     */
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent * event) override;

private:
    RangeGetter _range_getter;
    double _min_value = 0.0;
    double _max_value = 100.0;
    bool _use_getter = false;

    /// Optional axis mapping for domain↔world conversion and label formatting
    std::optional<CorePlotting::AxisMapping> _axis_mapping;

    /// When true, axis is inverted: min at top, max at bottom (image coordinates)
    bool _inverted = false;

    Neuralyzer::Plots::AxisDisplayMode _display_mode = Neuralyzer::Plots::AxisDisplayMode::Plot;
    int _thickness = 50;
    bool _show_extent_labels = true;
    QColor _negative_label_color{QColor(255, 153, 102)};
    Neuralyzer::Plots::AxisTickConfig _tick_config{};

    static constexpr int kTickWidth = 5;
    static constexpr int kMajorTickWidth = 8;
    static constexpr int kLabelOffset = 5;
    static constexpr int kPlotThickness = 50;
    static constexpr int kRulerThickness = 24;

    void _applyDisplayModeDefaults();

    /**
     * @brief Convert world Y value to pixel Y position
     * @param value World Y value
     * @param min Minimum world value
     * @param max Maximum world value
     * @return Pixel Y position (0 = top, height = bottom)
     */
    [[nodiscard]] int _valueToPixelY(double value, double min, double max) const;

    /**
     * @brief Resolve pen color for a tick value
     * @param value Tick coordinate
     * @param tick_interval Tick spacing
     * @param is_zero Whether this tick is at zero
     * @param is_major Whether this is a major tick
     * @return Pen color for the tick/label
     */
    [[nodiscard]] QColor _tickColor(double value, double tick_interval, bool is_zero, bool is_major) const;
};

#endif// VERTICAL_AXIS_WIDGET_HPP
