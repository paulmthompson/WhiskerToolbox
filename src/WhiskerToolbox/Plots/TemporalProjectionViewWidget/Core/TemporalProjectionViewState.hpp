#ifndef TEMPORAL_PROJECTION_VIEW_STATE_HPP
#define TEMPORAL_PROJECTION_VIEW_STATE_HPP

/**
 * @file TemporalProjectionViewState.hpp
 * @brief State class for TemporalProjectionViewWidget
 *
 * TemporalProjectionViewState manages the serializable state for the
 * TemporalProjectionViewWidget. View state (CorePlotting::ViewStateData) is
 * the single source of truth for zoom, pan, and data bounds; horizontal and
 * vertical axis states are kept in sync via setXBounds/setYBounds.
 *
 * @see EditorState for base class documentation
 * @see LinePlotState for the same pattern
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
#include <vector>

/**
 * @brief Serializable state data for TemporalProjectionViewWidget
 */
struct TemporalProjectionViewStateData {
    std::string instance_id;
    std::string display_name = "Spatial Overlay";
    CorePlotting::ViewStateData view_state;
    HorizontalAxisStateData horizontal_axis;
    VerticalAxisStateData vertical_axis;

    // Data keys
    std::vector<std::string> point_data_keys;
    std::vector<std::string> line_data_keys;
    // Note: mask_data_keys deferred due to CPU contour extraction performance

    // Rendering
    float point_size = 5.0f;
    float line_width = 2.0f;

    // Selection
    std::string selection_mode = "none"; // "none", "point", "line", "polygon"
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

    // === Data Key Management ===
    
    /** @brief Get all point data keys */
    [[nodiscard]] std::vector<QString> getPointDataKeys() const;
    
    /** @brief Add a point data key */
    void addPointDataKey(QString const & key);
    
    /** @brief Remove a point data key */
    void removePointDataKey(QString const & key);
    
    /** @brief Clear all point data keys */
    void clearPointDataKeys();
    
    /** @brief Get all line data keys */
    [[nodiscard]] std::vector<QString> getLineDataKeys() const;
    
    /** @brief Add a line data key */
    void addLineDataKey(QString const & key);
    
    /** @brief Remove a line data key */
    void removeLineDataKey(QString const & key);
    
    /** @brief Clear all line data keys */
    void clearLineDataKeys();

    // === Rendering Parameters ===
    
    /** @brief Get point size */
    [[nodiscard]] float getPointSize() const { return _data.point_size; }
    
    /** @brief Set point size */
    void setPointSize(float size);
    
    /** @brief Get line width */
    [[nodiscard]] float getLineWidth() const { return _data.line_width; }
    
    /** @brief Set line width */
    void setLineWidth(float width);

    // === Selection Mode ===
    
    /** @brief Get selection mode */
    [[nodiscard]] QString getSelectionMode() const { return QString::fromStdString(_data.selection_mode); }
    
    /** @brief Set selection mode ("none", "point", "line", "polygon") */
    void setSelectionMode(QString const & mode);

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

signals:
    void viewStateChanged();
    void pointDataKeyAdded(QString const & key);
    void pointDataKeyRemoved(QString const & key);
    void pointDataKeysCleared();
    void lineDataKeyAdded(QString const & key);
    void lineDataKeyRemoved(QString const & key);
    void lineDataKeysCleared();
    void pointSizeChanged(float size);
    void lineWidthChanged(float width);
    void selectionModeChanged(QString const & mode);

private:
    TemporalProjectionViewStateData _data;
    std::unique_ptr<HorizontalAxisState> _horizontal_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif  // TEMPORAL_PROJECTION_VIEW_STATE_HPP
