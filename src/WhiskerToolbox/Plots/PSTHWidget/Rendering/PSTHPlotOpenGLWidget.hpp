#ifndef PSTHPLOT_OPENGLWIDGET_HPP
#define PSTHPLOT_OPENGLWIDGET_HPP

/**
 * @file PSTHPlotOpenGLWidget.hpp
 * @brief OpenGL-based PSTH plot visualization widget
 * 
 * This widget renders Peri-Stimulus Time Histogram plots showing
 * event counts or rates aligned to a reference event.
 * 
 * Architecture:
 * - Receives PSTHState for alignment, bin size, and plot options
 * - Uses GatherResult<DigitalEventSeries> for trial-aligned data
 * - Uses PlottingOpenGL::SceneRenderer for OpenGL rendering
 * 
 * @see PlottingOpenGL/SceneRenderer.hpp
 */

#include "Core/PSTHState.hpp"

#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DataManager/utils/GatherResult.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>

class DataManager;
class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering PSTH plots
 * 
 * Displays histogram of event counts aligned to trial intervals.
 * 
 * Features:
 * - Time-aligned histogram visualization
 * - Configurable bin size
 * - Bar or line plot styles
 */
class PSTHPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PSTHPlotOpenGLWidget(QWidget * parent = nullptr);
    ~PSTHPlotOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    PSTHPlotOpenGLWidget(PSTHPlotOpenGLWidget const &) = delete;
    PSTHPlotOpenGLWidget & operator=(PSTHPlotOpenGLWidget const &) = delete;
    PSTHPlotOpenGLWidget(PSTHPlotOpenGLWidget &&) = delete;
    PSTHPlotOpenGLWidget & operator=(PSTHPlotOpenGLWidget &&) = delete;

    /**
     * @brief Set the PSTHState for this widget
     * 
     * The state provides alignment settings, bin size, and plot options.
     * The widget connects to state signals to react to changes.
     * 
     * @param state Shared pointer to PSTHState
     */
    void setState(std::shared_ptr<PSTHState> state);

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

private:
    // State management
    std::shared_ptr<PSTHState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // Rendering infrastructure
    PlottingOpenGL::SceneRenderer _scene_renderer;
    bool _opengl_initialized{false};
    bool _scene_dirty{true};

    // Widget dimensions
    int _widget_width{1};
    int _widget_height{1};

    // =========================================================================
    // Private Methods
    // =========================================================================

    /**
     * @brief Rebuild the renderable scene from current state
     * 
     * Gathers data aligned to trial intervals and builds histogram.
     */
    void rebuildScene();

    /**
     * @brief Convert screen coordinates to world coordinates
     */
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;
};

#endif // PSTHPLOT_OPENGLWIDGET_HPP
