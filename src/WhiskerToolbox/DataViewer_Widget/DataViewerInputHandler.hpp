#ifndef DATAVIEWER_INPUT_HANDLER_HPP
#define DATAVIEWER_INPUT_HANDLER_HPP

/**
 * @file DataViewerInputHandler.hpp
 * @brief Handles mouse input events for the DataViewer widget
 * 
 * This class extracts mouse event processing from OpenGLWidget to provide
 * a cleaner separation of concerns. It handles:
 * - Pan gesture detection and delta calculation
 * - Hover coordinate emission
 * - Click detection and routing
 * - Cursor shape management
 * 
 * The handler emits signals for various input events, which the parent widget
 * connects to for actual state changes and rendering updates.
 */

#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "CorePlotting/Interaction/HitTestResult.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "Entity/EntityRegistry.hpp"

#include <QObject>
#include <QPoint>

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>

class QMouseEvent;
class QWidget;

namespace DataViewer {

/**
 * @brief Context information needed by the input handler for coordinate transforms
 */
struct InputContext {
    CorePlotting::TimeSeriesViewState const * view_state{nullptr};
    CorePlotting::LayoutResponse const * layout_response{nullptr};
    CorePlotting::RenderableScene const * scene{nullptr};
    std::unordered_set<EntityId> const * selected_entities{nullptr};
    std::map<size_t, std::string> const * rectangle_batch_key_map{nullptr};
    int widget_width{0};
    int widget_height{0};
};

/**
 * @brief Handles mouse input events for the DataViewer widget
 * 
 * This class processes raw Qt mouse events and translates them into
 * semantic actions (pan, select, hover) that the parent widget can respond to.
 */
class DataViewerInputHandler : public QObject {
    Q_OBJECT

public:
    explicit DataViewerInputHandler(QWidget * parent = nullptr);
    ~DataViewerInputHandler() override = default;

    /**
     * @brief Update the context used for coordinate transforms
     * @param ctx Current context with view state, layout, and dimensions
     */
    void setContext(InputContext const & ctx);

    /**
     * @brief Process a mouse press event
     * @param event The Qt mouse event
     * @return true if the event was handled (should not propagate)
     */
    bool handleMousePress(QMouseEvent * event);

    /**
     * @brief Process a mouse move event
     * @param event The Qt mouse event
     * @return true if the event was handled
     */
    bool handleMouseMove(QMouseEvent * event);

    /**
     * @brief Process a mouse release event
     * @param event The Qt mouse event
     * @return true if the event was handled
     */
    bool handleMouseRelease(QMouseEvent * event);

    /**
     * @brief Process a mouse double-click event
     * @param event The Qt mouse event
     * @return true if the event was handled
     */
    bool handleDoubleClick(QMouseEvent * event);

    /**
     * @brief Process a leave event (mouse exits widget)
     */
    void handleLeave();

    /**
     * @brief Check if panning is currently active
     */
    [[nodiscard]] bool isPanning() const { return _is_panning; }

    /**
     * @brief Set whether an interaction is active (disables some input handling)
     */
    void setInteractionActive(bool active) { _interaction_active = active; }

    /**
     * @brief Set callback for series info lookup (used for hover display)
     */
    using SeriesInfoCallback = std::function<std::optional<std::pair<std::string, std::string>>(float, float)>;
    void setSeriesInfoCallback(SeriesInfoCallback callback) { _series_info_callback = std::move(callback); }

    /**
     * @brief Set callback for analog value lookup (used for hover display)
     */
    using AnalogValueCallback = std::function<float(float, std::string const &)>;
    void setAnalogValueCallback(AnalogValueCallback callback) { _analog_value_callback = std::move(callback); }

signals:
    // Pan gestures
    void panStarted();
    void panDelta(float normalized_dy);
    void panEnded();

    // Click events
    void clicked(float time_coord, float canvas_y, QString const & series_info);

    // Hover events
    void hoverCoordinates(float time_coord, float canvas_y, QString const & series_info);

    // Entity selection events
    void entityClicked(EntityId entity_id, bool ctrl_pressed);
    void entitySelectionCleared();

    // Interval edge events
    void intervalEdgeHovered(bool is_edge);
    void intervalEdgeDragRequested(CorePlotting::HitTestResult const & hit_result);

    // Double-click for interval creation
    void intervalCreationRequested(QString const & series_key, QPoint const & start_pos);

    // Cursor changes
    void cursorChangeRequested(Qt::CursorShape cursor);

    // Tooltip events
    void tooltipRequested(QPoint const & pos);
    void tooltipCancelled();

    // Repaint request
    void repaintRequested();

private:
    /**
     * @brief Convert canvas X to time coordinate
     */
    [[nodiscard]] float canvasXToTime(float canvas_x) const;

    /**
     * @brief Find interval edge at position using hit testing
     */
    [[nodiscard]] CorePlotting::HitTestResult findIntervalEdgeAtPosition(float canvas_x, float canvas_y) const;

    /**
     * @brief Perform hit testing at position
     */
    [[nodiscard]] CorePlotting::HitTestResult hitTestAtPosition(float canvas_x, float canvas_y) const;

    /**
     * @brief Build series info string for the first visible analog series
     */
    [[nodiscard]] QString buildSeriesInfoString(float canvas_x, float canvas_y) const;

    InputContext _ctx;
    QWidget * _parent_widget{nullptr};

    // Pan state
    bool _is_panning{false};
    QPoint _last_mouse_pos;

    // Interaction state (set by external manager)
    bool _interaction_active{false};

    // Callbacks for data lookup
    SeriesInfoCallback _series_info_callback;
    AnalogValueCallback _analog_value_callback;
};

}// namespace DataViewer

#endif// DATAVIEWER_INPUT_HANDLER_HPP
