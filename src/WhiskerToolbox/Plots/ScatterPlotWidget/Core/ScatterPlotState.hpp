#ifndef SCATTER_PLOT_STATE_HPP
#define SCATTER_PLOT_STATE_HPP

/**
 * @file ScatterPlotState.hpp
 * @brief State class for ScatterPlotWidget
 *
 * ScatterPlotState manages the serializable state for the ScatterPlotWidget,
 * with a single source of truth for view state (zoom/pan) and axis ranges.
 * HorizontalAxisState and VerticalAxisState hold full axis ranges; view state
 * holds zoom/pan. Both axes are analog value axes (no time).
 *
 * @see EditorState for base class documentation
 * @see PSTHState, LinePlotState for the same pattern
 */

#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "EditorState/EditorState.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisStateData.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisStateData.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>

/**
 * @brief Serializable state data for ScatterPlotWidget
 */
struct ScatterPlotStateData {
    std::string instance_id;
    std::string display_name = "Scatter Plot";
    CorePlotting::ViewStateData view_state;
    HorizontalAxisStateData horizontal_axis;
    VerticalAxisStateData vertical_axis;
};

/**
 * @brief State class for ScatterPlotWidget
 *
 * Single source of truth: view_state (zoom/pan) plus horizontal and vertical
 * axis states (full range). OpenGL widget and axis widgets read from state.
 */
class ScatterPlotState : public EditorState {
    Q_OBJECT

public:
    explicit ScatterPlotState(QObject * parent = nullptr);
    ~ScatterPlotState() override = default;

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("ScatterPlot"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Axis state access (for widgets and serialization) ===
    [[nodiscard]] HorizontalAxisState * horizontalAxisState() { return _horizontal_axis_state.get(); }
    [[nodiscard]] VerticalAxisState * verticalAxisState() { return _vertical_axis_state.get(); }

    // === Legacy accessors (delegate to axis states) ===
    [[nodiscard]] double getXMin() const;
    [[nodiscard]] double getXMax() const;
    [[nodiscard]] double getYMin() const;
    [[nodiscard]] double getYMax() const;

    // === View state (zoom / pan / bounds) ===
    /** @brief Get the current view state (zoom, pan, data bounds). */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const { return _data.view_state; }
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);
    /** @brief Set X data bounds; updates view state and horizontal axis. */
    void setXBounds(double x_min, double x_max);
    /** @brief Set Y data bounds; updates view state and vertical axis. */
    void setYBounds(double y_min, double y_max);

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

signals:
    void viewStateChanged();

private:
    ScatterPlotStateData _data;
    std::unique_ptr<HorizontalAxisState> _horizontal_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif  // SCATTER_PLOT_STATE_HPP
