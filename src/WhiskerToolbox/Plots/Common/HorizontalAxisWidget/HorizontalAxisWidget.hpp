#ifndef HORIZONTAL_AXIS_WIDGET_HPP
#define HORIZONTAL_AXIS_WIDGET_HPP

/**
 * @file HorizontalAxisWidget.hpp
 * @brief Widget for rendering a horizontal axis with tick marks
 *
 * This widget displays a horizontal axis with tick marks and labels
 * showing world coordinate values. It can be used for X-axis display
 * in various plot widgets and as a top ruler in the Media Viewer.
 */

#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "Common/AxisTickLayout.hpp"

#include <QColor>
#include <QWidget>

#include <functional>
#include <optional>

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
     * @brief Set plot or ruler display mode
     * @param mode Display mode
     */
    void setDisplayMode(Neuralyzer::Plots::AxisDisplayMode mode);

    /**
     * @brief Set the widget thickness in pixels
     * @param px Height in pixels
     */
    void setThickness(int px);

    /**
     * @brief Show or hide min/max extent labels at the bottom
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

    /// Optional axis mapping for domain↔world conversion and label formatting
    std::optional<CorePlotting::AxisMapping> _axis_mapping;

    Neuralyzer::Plots::AxisDisplayMode _display_mode = Neuralyzer::Plots::AxisDisplayMode::Plot;
    int _thickness = 50;
    bool _show_extent_labels = true;
    QColor _negative_label_color{QColor(255, 153, 102)};
    Neuralyzer::Plots::AxisTickConfig _tick_config{};

    static constexpr int kTickHeight = 5;
    static constexpr int kMajorTickHeight = 8;
    static constexpr int kLabelOffset = 5;
    static constexpr int kPlotThickness = 50;
    static constexpr int kRulerThickness = 24;

    void _applyDisplayModeDefaults();

    /**
     * @brief Convert world X value to pixel X position
     * @param value World X value
     * @param min Minimum world value
     * @param max Maximum world value
     * @return Pixel X position (0 = left, width = right)
     */
    [[nodiscard]] int _valueToPixelX(double value, double min, double max) const;

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

#endif// HORIZONTAL_AXIS_WIDGET_HPP
