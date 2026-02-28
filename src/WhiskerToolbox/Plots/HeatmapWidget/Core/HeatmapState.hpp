#ifndef HEATMAP_STATE_HPP
#define HEATMAP_STATE_HPP

/**
 * @file HeatmapState.hpp
 * @brief State class for HeatmapWidget
 *
 * HeatmapState manages the serializable state for the HeatmapWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 *
 * View state uses CorePlotting::ViewStateData as the single source of truth.
 * Y bounds are stored in view state (world [-1, 1] for trial index axis) and kept
 * in sync with the vertical axis state.
 *
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "Plots/Common/EventRateEstimation/RateEstimate.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisStateData.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisStateData.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>
#include <vector>

// =============================================================================
// Color Range Configuration (serializable)
// =============================================================================

/**
 * @brief Configuration for how the heatmap color range is determined
 *
 * This mirrors CorePlotting::HeatmapColorRange but lives alongside
 * HeatmapStateData so it can be serialized via rfl::json.
 */
struct HeatmapColorRangeConfig {
    enum class Mode {
        Auto,       ///< Range from data min/max (default)
        Manual,     ///< User-specified vmin/vmax
        Symmetric,  ///< Symmetric around zero (for z-scores)
    };

    Mode mode = Mode::Auto;
    double vmin = 0.0;   ///< Manual minimum (used when mode == Manual)
    double vmax = 1.0;   ///< Manual maximum (used when mode == Manual)
};

// =============================================================================
// State Data
// =============================================================================

/**
 * @brief Serializable state data for HeatmapWidget
 */
struct HeatmapStateData {
    std::string instance_id;
    std::string display_name = "Heatmap Plot";
    PlotAlignmentData alignment;
    CorePlotting::ViewStateData view_state;
    RelativeTimeAxisStateData time_axis;
    VerticalAxisStateData vertical_axis;
    std::string background_color = "#FFFFFF";
    std::vector<std::string> unit_keys;  ///< Selected DigitalEventSeries keys
    WhiskerToolbox::Plots::ScalingMode scaling =
            WhiskerToolbox::Plots::ScalingMode::FiringRateHz;
    HeatmapColorRangeConfig color_range;
};

/**
 * @brief State class for HeatmapWidget
 *
 * View state is the single source of truth for zoom/pan. RelativeTimeAxisState
 * and VerticalAxisState are kept in sync with the view state.
 */
class HeatmapState : public EditorState {
    Q_OBJECT

public:
    explicit HeatmapState(QObject * parent = nullptr);
    ~HeatmapState() override = default;

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("HeatmapWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Alignment ===
    [[nodiscard]] QString getAlignmentEventKey() const;
    void setAlignmentEventKey(QString const & key);
    [[nodiscard]] IntervalAlignmentType getIntervalAlignmentType() const;
    void setIntervalAlignmentType(IntervalAlignmentType type);
    [[nodiscard]] double getOffset() const;
    void setOffset(double offset);
    [[nodiscard]] double getWindowSize() const;
    void setWindowSize(double window_size);
    [[nodiscard]] PlotAlignmentState * alignmentState() { return _alignment_state.get(); }
    [[nodiscard]] RelativeTimeAxisState * relativeTimeAxisState() {
        return _relative_time_axis_state.get();
    }
    [[nodiscard]] VerticalAxisState * verticalAxisState() { return _vertical_axis_state.get(); }

    // === View State ===
    /** @brief Get the current view state */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const { return _data.view_state; }
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);
    /** @brief Set X data bounds. Emits viewStateChanged() AND stateChanged(). */
    void setXBounds(double x_min, double x_max);
    /** @brief Set Y data bounds. Emits viewStateChanged() AND stateChanged(). */
    void setYBounds(double y_min, double y_max);

    // === Unit Management ===
    /** @brief Get the list of selected DigitalEventSeries keys */
    [[nodiscard]] std::vector<std::string> const & unitKeys() const { return _data.unit_keys; }
    /** @brief Add a DigitalEventSeries key to the unit list */
    void addUnit(std::string const & key);
    /** @brief Remove a DigitalEventSeries key from the unit list */
    void removeUnit(std::string const & key);
    /** @brief Add multiple DigitalEventSeries keys at once */
    void addUnits(std::vector<std::string> const & keys);
    /** @brief Remove multiple DigitalEventSeries keys at once */
    void removeUnits(std::vector<std::string> const & keys);
    /** @brief Check if a unit key is currently selected */
    [[nodiscard]] bool hasUnit(std::string const & key) const;
    /** @brief Get the number of selected units */
    [[nodiscard]] size_t unitCount() const { return _data.unit_keys.size(); }

    // === Scaling ===
    /** @brief Get the current scaling mode */
    [[nodiscard]] WhiskerToolbox::Plots::ScalingMode scaling() const { return _data.scaling; }
    /** @brief Set the scaling mode. Emits scalingChanged() and stateChanged(). */
    void setScaling(WhiskerToolbox::Plots::ScalingMode scaling);

    // === Color Range ===
    /** @brief Get the current color range configuration */
    [[nodiscard]] HeatmapColorRangeConfig const & colorRange() const { return _data.color_range; }
    /** @brief Set the color range mode. Emits colorRangeChanged() and stateChanged(). */
    void setColorRangeMode(HeatmapColorRangeConfig::Mode mode);
    /** @brief Set the manual color range bounds. Emits colorRangeChanged() and stateChanged(). */
    void setColorRangeBounds(double vmin, double vmax);
    /** @brief Set the full color range config. Emits colorRangeChanged() and stateChanged(). */
    void setColorRange(HeatmapColorRangeConfig const & config);

    // === Background Color ===
    [[nodiscard]] QString getBackgroundColor() const;
    void setBackgroundColor(QString const & hex_color);

    // === Direct Data Access ===
    [[nodiscard]] HeatmapStateData const & data() const { return _data; }

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

signals:
    void alignmentEventKeyChanged(QString const & key);
    void intervalAlignmentTypeChanged(IntervalAlignmentType type);
    void offsetChanged(double offset);
    void windowSizeChanged(double window_size);
    void viewStateChanged();
    void backgroundColorChanged(QString const & hex_color);
    void unitsChanged();  ///< Emitted when the unit list changes (add/remove)
    void scalingChanged();     ///< Emitted when scaling mode changes
    void colorRangeChanged();  ///< Emitted when color range config changes

private:
    HeatmapStateData _data;
    std::unique_ptr<PlotAlignmentState> _alignment_state;
    std::unique_ptr<RelativeTimeAxisState> _relative_time_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif// HEATMAP_STATE_HPP
