#ifndef ONION_SKIN_VIEW_STATE_HPP
#define ONION_SKIN_VIEW_STATE_HPP

/**
 * @file OnionSkinViewState.hpp
 * @brief State class for OnionSkinViewWidget
 *
 * OnionSkinViewState manages the serializable state for the
 * OnionSkinViewWidget. View state (CorePlotting::ViewStateData) is
 * the single source of truth for zoom, pan, and data bounds; horizontal and
 * vertical axis states are kept in sync via setXBounds/setYBounds.
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
 * @brief Serializable state data for OnionSkinViewWidget
 */
struct OnionSkinViewStateData {
    std::string instance_id;
    std::string display_name = "Onion Skin View";
    CorePlotting::ViewStateData view_state;
    HorizontalAxisStateData horizontal_axis;
    VerticalAxisStateData vertical_axis;
};

/**
 * @brief State class for OnionSkinViewWidget
 *
 * Single source of truth: view_state (zoom/pan) plus horizontal and vertical
 * axis states (full range). OpenGL widget and axis widgets read from state.
 */
class OnionSkinViewState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new OnionSkinViewState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit OnionSkinViewState(QObject * parent = nullptr);

    ~OnionSkinViewState() override = default;

    // === Type Identification ===

    [[nodiscard]] QString getTypeName() const override
    {
        return QStringLiteral("OnionSkinView");
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

    // === View state (zoom / pan / bounds) ===
    /** @brief Get the current view state (bounds + zoom + pan). */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const
    {
        return _data.view_state;
    }
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);

    /**
     * @brief Set X data bounds and keep horizontal axis in sync.
     * Call when the plot updates the horizontal axis from data (e.g. after a rebuild).
     * @param x_min Minimum X value
     * @param x_max Maximum X value
     */
    void setXBounds(double x_min, double x_max);

    /**
     * @brief Set Y data bounds and keep vertical axis in sync.
     * Call when the plot updates the vertical axis from data (e.g. after a rebuild).
     * @param y_min Minimum Y value
     * @param y_max Maximum Y value
     */
    void setYBounds(double y_min, double y_max);

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

signals:
    void viewStateChanged();

private:
    OnionSkinViewStateData _data;
    std::unique_ptr<HorizontalAxisState> _horizontal_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif  // ONION_SKIN_VIEW_STATE_HPP
