#ifndef LINEPLOT_OPENGLWIDGET_HPP
#define LINEPLOT_OPENGLWIDGET_HPP

/**
 * @file LinePlotOpenGLWidget.hpp
 * @brief OpenGL-based line plot visualization using CorePlotting infrastructure
 * 
 * This widget renders AnalogTimeSeries data as line plots,
 * aligned to trial intervals specified via LinePlotState.
 * 
 * Architecture:
 * - Receives LinePlotState for alignment, view settings, and line options
 * - Uses GatherResult<AnalogTimeSeries> for trial-aligned data
 * - Uses CorePlotting::TimeSeriesMapper for coordinate mapping
 * - Uses PlottingOpenGL::SceneRenderer for OpenGL rendering
 * 
 * @see CorePlotting/Mappers/TimeSeriesMapper.hpp
 * @see PlottingOpenGL/SceneRenderer.hpp
 */

#include "Core/LinePlotState.hpp"

#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DataManager/utils/GatherResult.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <vector>

class DataManager;
class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering line plots
 * 
 * Displays AnalogTimeSeries data aligned to trial intervals. Each trial
 * is shown as a line plot with values rendered at their relative time positions.
 * 
 * Features:
 * - Independent X (time) and Y (value) zooming
 * - Panning with mouse drag
 * - Wheel zoom (Shift+wheel for Y-only)
 * - Hover detection with tooltips
 */
class LinePlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit LinePlotOpenGLWidget(QWidget * parent = nullptr);
    ~LinePlotOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    LinePlotOpenGLWidget(LinePlotOpenGLWidget const &) = delete;
    LinePlotOpenGLWidget & operator=(LinePlotOpenGLWidget const &) = delete;
    LinePlotOpenGLWidget(LinePlotOpenGLWidget &&) = delete;
    LinePlotOpenGLWidget & operator=(LinePlotOpenGLWidget &&) = delete;

    /**
     * @brief Set the LinePlotState for this widget
     * 
     * The state provides alignment settings, view configuration, and line options.
     * The widget connects to state signals to react to changes.
     * 
     * @param state Shared pointer to LinePlotState
     */
    void setState(std::shared_ptr<LinePlotState> state);

    /**
     * @brief Set the DataManager for data access
     * @param data_manager Shared pointer to DataManager
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Get the current view bounds (for RelativeTimeAxisWidget)
     * @return View bounds based on alignment window size
     */
    [[nodiscard]] std::pair<double, double> getViewBounds() const;

signals:
    /**
     * @brief Emitted when user double-clicks on the plot
     * @param time_frame_index The time frame index at the click position
     */
    void plotDoubleClicked(int64_t time_frame_index);

    /**
     * @brief Emitted when view bounds change (window size changes)
     */
    void viewBoundsChanged();

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

private slots:
    /**
     * @brief Rebuild scene when state changes
     */
    void onStateChanged();

    /**
     * @brief Handle window size changes
     */
    void onWindowSizeChanged(double window_size);

private:
    // State management
    std::shared_ptr<LinePlotState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // Rendering infrastructure
    PlottingOpenGL::SceneRenderer _scene_renderer;
    CorePlotting::RenderableScene _scene;
    bool _scene_dirty{true};
    bool _opengl_initialized{false};

    // View matrices
    glm::mat4 _view_matrix{1.0f};
    glm::mat4 _projection_matrix{1.0f};

    // Interaction state
    bool _is_panning{false};
    QPoint _last_mouse_pos;

    // Widget dimensions
    int _widget_width{1};
    int _widget_height{1};

    // =========================================================================
    // Private Methods
    // =========================================================================

    /**
     * @brief Rebuild the renderable scene from current state
     * 
     * Gathers data aligned to trial intervals and builds line batches.
     */
    void rebuildScene();

    /**
     * @brief Update view and projection matrices from window size
     */
    void updateMatrices();

    /**
     * @brief Convert screen coordinates to world coordinates
     */
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;

    /**
     * @brief Handle panning motion
     */
    void handlePanning(int delta_x, int delta_y);

    /**
     * @brief Handle zoom via wheel
     */
    void handleZoom(float delta, bool y_only);

    /**
     * @brief Gather trial-aligned data for building scene
     */
    [[nodiscard]] GatherResult<AnalogTimeSeries> gatherTrialData() const;
};

#endif // LINEPLOT_OPENGLWIDGET_HPP
