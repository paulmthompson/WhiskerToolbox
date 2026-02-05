#ifndef EVENTPLOT_OPENGLWIDGET_HPP
#define EVENTPLOT_OPENGLWIDGET_HPP

/**
 * @file EventPlotOpenGLWidget.hpp
 * @brief OpenGL-based raster plot visualization using CorePlotting infrastructure
 * 
 * This widget renders DigitalEventSeries data as a raster plot (PSTH style),
 * aligned to trial intervals specified via EventPlotState.
 * 
 * Architecture:
 * - Receives EventPlotState for alignment, view settings, and glyph options
 * - Uses GatherResult<DigitalEventSeries> for trial-aligned data
 * - Uses CorePlotting::RasterMapper for coordinate mapping
 * - Uses CorePlotting::RowLayoutStrategy for Y positioning
 * - Uses PlottingOpenGL::SceneRenderer for OpenGL rendering
 * 
 * @see CorePlotting/Mappers/RasterMapper.hpp
 * @see PlottingOpenGL/SceneRenderer.hpp
 */

#include "Core/EventPlotState.hpp"

#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/RowLayoutStrategy.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DataManager/utils/GatherResult.hpp"

#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QTimer>

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <vector>

class DataManager;
class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering event raster plots
 * 
 * Displays DigitalEventSeries data aligned to trial intervals. Each trial
 * is shown as a horizontal row with events rendered as glyphs at their
 * relative time positions.
 * 
 * Features:
 * - Independent X (time) and Y (trial) zooming
 * - Panning with mouse drag
 * - Wheel zoom (Shift+wheel for Y-only)
 * - Hover detection with tooltips
 * - Hit testing for event selection
 */
class EventPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit EventPlotOpenGLWidget(QWidget * parent = nullptr);
    ~EventPlotOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    EventPlotOpenGLWidget(EventPlotOpenGLWidget const &) = delete;
    EventPlotOpenGLWidget & operator=(EventPlotOpenGLWidget const &) = delete;
    EventPlotOpenGLWidget(EventPlotOpenGLWidget &&) = delete;
    EventPlotOpenGLWidget & operator=(EventPlotOpenGLWidget &&) = delete;

    /**
     * @brief Set the EventPlotState for this widget
     * 
     * The state provides alignment settings, view configuration, and glyph options.
     * The widget connects to state signals to react to changes.
     * 
     * @param state Shared pointer to EventPlotState
     */
    void setState(std::shared_ptr<EventPlotState> state);

    /**
     * @brief Set the DataManager for data access
     * @param data_manager Shared pointer to DataManager
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Get the current view state (zoom, pan, bounds)
     */
    [[nodiscard]] EventPlotViewState const & viewState() const;

    /**
     * @brief Reset view to default bounds (fit data)
     */
    void resetView();

    /**
     * @brief Enable or disable tooltips
     */
    void setTooltipsEnabled(bool enabled);

signals:
    /**
     * @brief Emitted when user double-clicks on an event
     * @param time_frame_index The time frame index of the event
     * @param series_key The key of the event series
     */
    void eventDoubleClicked(int64_t time_frame_index, QString const & series_key);

    /**
     * @brief Emitted when mouse moves over event area
     * @param world_x World X coordinate (relative time)
     * @param world_y World Y coordinate (trial index)
     */
    void mouseWorldMoved(float world_x, float world_y);

    /**
     * @brief Emitted when view bounds change (zoom/pan)
     */
    void viewBoundsChanged();

    /**
     * @brief Emitted when trial count changes (after scene rebuild)
     * @param count Number of trials
     */
    void trialCountChanged(size_t count);

protected:
    // QOpenGLWidget overrides
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    // Mouse interaction
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void leaveEvent(QEvent * event) override;

private slots:
    /**
     * @brief Rebuild scene when state changes
     */
    void onStateChanged();

    /**
     * @brief Update glyph appearance when view state changes
     */
    void onViewStateChanged();

    /**
     * @brief Handle tooltip timer
     */
    void onTooltipTimer();

private:
    // State management
    std::shared_ptr<EventPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // Rendering infrastructure
    PlottingOpenGL::SceneRenderer _scene_renderer;
    CorePlotting::RenderableScene _scene;
    CorePlotting::RowLayoutStrategy _layout_strategy;
    bool _scene_dirty{true};
    bool _opengl_initialized{false};

    // View state (cached from EventPlotState for rendering)
    EventPlotViewState _cached_view_state;
    glm::mat4 _view_matrix{1.0f};
    glm::mat4 _projection_matrix{1.0f};

    // Interaction state
    bool _is_panning{false};
    QPoint _last_mouse_pos;
    bool _tooltips_enabled{true};
    QTimer * _tooltip_timer{nullptr};
    std::optional<QPoint> _pending_tooltip_pos;

    // Widget dimensions
    int _widget_width{1};
    int _widget_height{1};

    // =========================================================================
    // Private Methods
    // =========================================================================

    /**
     * @brief Rebuild the renderable scene from current state
     * 
     * Gathers data aligned to trial intervals and builds glyph batches.
     */
    void rebuildScene();

    /**
     * @brief Update view and projection matrices from view state
     */
    void updateMatrices();

    /**
     * @brief Convert screen coordinates to world coordinates
     */
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;

    /**
     * @brief Convert world coordinates to screen coordinates
     */
    [[nodiscard]] QPoint worldToScreen(float world_x, float world_y) const;

    /**
     * @brief Handle panning motion
     */
    void handlePanning(int delta_x, int delta_y);

    /**
     * @brief Handle zoom via wheel
     */
    void handleZoom(float delta, bool y_only);

    /**
     * @brief Find event near screen position (for hit testing)
     * @return Pair of (trial_index, event_index) or nullopt if none
     */
    [[nodiscard]] std::optional<std::pair<int, int>> findEventNear(
        QPoint const & screen_pos, float tolerance_pixels = 10.0f) const;

    /**
     * @brief Render the center line at t=0
     */
    void renderCenterLine();

    /**
     * @brief Render axis labels and tick marks
     */
    void renderAxes();

    /**
     * @brief Gather trial-aligned data for building scene
     */
    [[nodiscard]] GatherResult<DigitalEventSeries> gatherTrialData() const;

    /**
     * @brief Update OpenGL clear color from state background color
     */
    void updateBackgroundColor();
};

#endif // EVENTPLOT_OPENGLWIDGET_HPP
