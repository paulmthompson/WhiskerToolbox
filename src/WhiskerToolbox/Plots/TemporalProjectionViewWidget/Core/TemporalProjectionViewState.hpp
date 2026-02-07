#ifndef TEMPORAL_PROJECTION_VIEW_STATE_HPP
#define TEMPORAL_PROJECTION_VIEW_STATE_HPP

/**
 * @file TemporalProjectionViewState.hpp
 * @brief State class for TemporalProjectionViewWidget
 *
 * TemporalProjectionViewState manages the serializable state for the
 * TemporalProjectionViewWidget, with a single source of truth for view state
 * (zoom/pan) and axis ranges. HorizontalAxisState and VerticalAxisState hold
 * full axis ranges; view state holds zoom/pan.
 *
 * @see EditorState for base class documentation
 * @see ScatterPlotState for the same pattern
 */

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
 * @brief View state for the temporal projection view (zoom and pan only)
 *
 * Data bounds come from HorizontalAxisState and VerticalAxisState.
 * This struct only holds the view transform.
 */
struct TemporalProjectionViewViewState {
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    double x_pan = 0.0;
    double y_pan = 0.0;
};

/**
 * @brief Serializable state data for TemporalProjectionViewWidget
 */
struct TemporalProjectionViewStateData {
    std::string instance_id;
    std::string display_name = "Temporal Projection View";
    TemporalProjectionViewViewState view_state;
    HorizontalAxisStateData horizontal_axis;
    VerticalAxisStateData vertical_axis;
};

/**
 * @brief State class for TemporalProjectionViewWidget
 *
 * Single source of truth: view_state (zoom/pan) plus horizontal and vertical
 * axis states (full range). OpenGL widget and axis widgets read from state.
 */
class TemporalProjectionViewState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TemporalProjectionViewState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit TemporalProjectionViewState(QObject * parent = nullptr);

    ~TemporalProjectionViewState() override = default;

    // === Type Identification ===

    [[nodiscard]] QString getTypeName() const override
    {
        return QStringLiteral("TemporalProjectionView");
    }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Axis state access (for widgets and serialization) ===
    [[nodiscard]] HorizontalAxisState * horizontalAxisState()
    {
        return _horizontal_axis_state.get();
    }
    [[nodiscard]] VerticalAxisState * verticalAxisState()
    {
        return _vertical_axis_state.get();
    }

    // === Legacy accessors (delegate to axis states) ===
    [[nodiscard]] double getXMin() const;
    [[nodiscard]] double getXMax() const;
    [[nodiscard]] double getYMin() const;
    [[nodiscard]] double getYMax() const;

    // === View state (zoom / pan) ===
    [[nodiscard]] TemporalProjectionViewViewState const & viewState() const
    {
        return _data.view_state;
    }
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

signals:
    void viewStateChanged();

private:
    TemporalProjectionViewStateData _data;
    std::unique_ptr<HorizontalAxisState> _horizontal_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif  // TEMPORAL_PROJECTION_VIEW_STATE_HPP
