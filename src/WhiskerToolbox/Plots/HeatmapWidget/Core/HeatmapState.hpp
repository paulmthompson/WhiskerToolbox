#ifndef HEATMAP_STATE_HPP
#define HEATMAP_STATE_HPP

/**
 * @file HeatmapState.hpp
 * @brief State class for HeatmapWidget
 *
 * HeatmapState manages the serializable state for the HeatmapWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 *
 * The view state follows the same single-source-of-truth pattern as EventPlotState:
 * - x_min/x_max define data bounds (set by window size)
 * - x_zoom/y_zoom/x_pan/y_pan define the view transform
 * - RelativeTimeAxisState is kept in sync bidirectionally
 *
 * @see EditorState for base class documentation
 * @see EventPlotState for the pattern this follows
 */

#include "EditorState/EditorState.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisStateData.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>

/**
 * @brief View state for the heatmap plot
 *
 * Follows the same pattern as EventPlotViewState:
 * - x_min/x_max are data bounds (changed when window size changes -> scene rebuild)
 * - x_zoom/y_zoom/x_pan/y_pan are view transform (only changes projection matrix)
 */
struct HeatmapViewState {
    // === Data Bounds (changing these triggers scene rebuild) ===
    double x_min = -500.0;
    double x_max = 500.0;

    // === View Transform (changing these only updates projection matrix) ===
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    double x_pan = 0.0;
    double y_pan = 0.0;
};

/**
 * @brief Serializable state data for HeatmapWidget
 */
struct HeatmapStateData {
    std::string instance_id;
    std::string display_name = "Heatmap Plot";
    PlotAlignmentData alignment;
    HeatmapViewState view_state;
    RelativeTimeAxisStateData time_axis;
    std::string background_color = "#FFFFFF";
};

/**
 * @brief State class for HeatmapWidget
 *
 * View state is the single source of truth for zoom/pan. The RelativeTimeAxisState
 * is kept in bidirectional sync with the view state.
 */
class HeatmapState : public EditorState {
    Q_OBJECT

public:
    explicit HeatmapState(QObject * parent = nullptr);
    ~HeatmapState() override = default;

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Heatmap"); }
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
    [[nodiscard]] RelativeTimeAxisState * relativeTimeAxisState() { return _relative_time_axis_state.get(); }

    // === View State ===
    [[nodiscard]] HeatmapViewState const & viewState() const { return _data.view_state; }
    void setViewState(HeatmapViewState const & view_state);
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);
    void setXBounds(double x_min, double x_max);

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

private:
    HeatmapStateData _data;
    std::unique_ptr<PlotAlignmentState> _alignment_state;
    std::unique_ptr<RelativeTimeAxisState> _relative_time_axis_state;
};

#endif// HEATMAP_STATE_HPP
