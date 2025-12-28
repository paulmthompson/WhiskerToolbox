#ifndef DATAVIEWER_INTERACTION_MANAGER_HPP
#define DATAVIEWER_INTERACTION_MANAGER_HPP

/**
 * @file DataViewerInteractionManager.hpp
 * @brief Manages interaction state machine for the DataViewer widget
 * 
 * This class extracts the interaction logic from OpenGLWidget, handling:
 * - Interaction mode transitions (Normal, CreateInterval, ModifyInterval, etc.)
 * - Controller lifecycle for interval creation and edge dragging
 * - Preview geometry generation
 * - Coordinate conversion for committing interactions
 * 
 * The manager emits signals when interactions complete, allowing the parent
 * widget to update the DataManager accordingly.
 */

#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "CorePlotting/Interaction/DataCoordinates.hpp"
#include "CorePlotting/Interaction/HitTestResult.hpp"
#include "CorePlotting/Interaction/IGlyphInteractionController.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "DataViewerCoordinates.hpp"

#include <QObject>

#include <glm/glm.hpp>

#include <memory>
#include <optional>
#include <string>

namespace DataViewer {

/**
 * @brief Interaction mode for the DataViewer widget
 */
enum class InteractionMode {
    Normal,        ///< Default: pan, select, hover tooltips
    CreateInterval,///< Click-drag to create a new interval
    ModifyInterval,///< Edge dragging to modify existing interval
    CreateLine,    ///< Click-drag to draw a selection line (future)
};

/**
 * @brief Context information needed for coordinate conversions
 */
struct InteractionContext {
    CorePlotting::TimeSeriesViewState const * view_state{nullptr};
    CorePlotting::RenderableScene const * scene{nullptr};
    int widget_width{0};
    int widget_height{0};
    
    /**
     * @brief Create a DataViewerCoordinates instance from this context
     * @return DataViewerCoordinates configured with current view state and dimensions
     */
    [[nodiscard]] DataViewerCoordinates makeCoordinates() const {
        if (view_state) {
            return DataViewerCoordinates(*view_state, widget_width, widget_height);
        }
        return DataViewerCoordinates();
    }
};

/**
 * @brief Manages interaction state machine for creating and modifying glyphs
 */
class DataViewerInteractionManager : public QObject {
    Q_OBJECT

public:
    explicit DataViewerInteractionManager(QObject * parent = nullptr);
    ~DataViewerInteractionManager() override = default;

    /**
     * @brief Update the context used for coordinate conversions
     */
    void setContext(InteractionContext const & ctx);

    // ========================================================================
    // Mode Management
    // ========================================================================

    /**
     * @brief Set the current interaction mode
     * 
     * Changes how mouse events are interpreted. When switching modes,
     * any active interaction is cancelled.
     */
    void setMode(InteractionMode mode);

    /**
     * @brief Get the current interaction mode
     */
    [[nodiscard]] InteractionMode mode() const { return _mode; }

    /**
     * @brief Check if any interaction is currently active
     */
    [[nodiscard]] bool isActive() const;

    /**
     * @brief Cancel any active interaction without committing
     */
    void cancel();

    // ========================================================================
    // Interval Creation
    // ========================================================================

    /**
     * @brief Start creating a new interval
     * 
     * @param series_key The key identifying the target series
     * @param canvas_x Starting X position in canvas coordinates
     * @param canvas_y Starting Y position in canvas coordinates
     * @param fill_color Fill color for the preview
     * @param stroke_color Stroke color for the preview
     */
    void startIntervalCreation(
            std::string const & series_key,
            float canvas_x, float canvas_y,
            glm::vec4 const & fill_color,
            glm::vec4 const & stroke_color);

    // ========================================================================
    // Interval Edge Dragging
    // ========================================================================

    /**
     * @brief Start dragging an interval edge
     * 
     * @param hit_result The hit test result identifying the edge
     * @param fill_color Fill color for the preview
     * @param stroke_color Stroke color for the preview
     */
    void startEdgeDrag(
            CorePlotting::HitTestResult const & hit_result,
            glm::vec4 const & fill_color,
            glm::vec4 const & stroke_color);

    // ========================================================================
    // Interaction Updates
    // ========================================================================

    /**
     * @brief Update the current interaction with new mouse position
     * 
     * @param canvas_x Current X position in canvas coordinates
     * @param canvas_y Current Y position in canvas coordinates
     */
    void update(float canvas_x, float canvas_y);

    /**
     * @brief Complete the current interaction
     * 
     * Converts preview geometry to data coordinates and emits
     * interactionCompleted signal.
     */
    void complete();

    // ========================================================================
    // Preview Access
    // ========================================================================

    /**
     * @brief Get the current preview geometry for rendering
     * @return Preview if an interaction is active, nullopt otherwise
     */
    [[nodiscard]] std::optional<CorePlotting::Interaction::GlyphPreview> getPreview() const;

signals:
    /**
     * @brief Emitted when the interaction mode changes
     */
    void modeChanged(DataViewer::InteractionMode mode);

    /**
     * @brief Emitted when an interaction completes successfully
     * 
     * The DataCoordinates contain all information needed to update the DataManager.
     */
    void interactionCompleted(CorePlotting::Interaction::DataCoordinates const & coords);

    /**
     * @brief Emitted when the preview geometry changes
     */
    void previewUpdated();

    /**
     * @brief Emitted when cursor shape should change
     */
    void cursorChangeRequested(Qt::CursorShape cursor);

private:
    /**
     * @brief Convert preview to data coordinates based on type
     */
    [[nodiscard]] CorePlotting::Interaction::DataCoordinates convertPreviewToDataCoords() const;

    InteractionContext _ctx;
    InteractionMode _mode{InteractionMode::Normal};

    std::unique_ptr<CorePlotting::Interaction::IGlyphInteractionController> _controller;
    std::string _series_key;
};

}// namespace DataViewer

#endif// DATAVIEWER_INTERACTION_MANAGER_HPP
