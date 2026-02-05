#include "HeatmapOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "CoreGeometry/boundingbox.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

HeatmapOpenGLWidget::HeatmapOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent)
{
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Request OpenGL 4.1 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling
    setFormat(format);
}

HeatmapOpenGLWidget::~HeatmapOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void HeatmapOpenGLWidget::setState(std::shared_ptr<HeatmapState> state)
{
    // Disconnect old state signals
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        // Connect to state signals
        connect(_state.get(), &HeatmapState::stateChanged,
                this, &HeatmapOpenGLWidget::onStateChanged);
        connect(_state.get(), &HeatmapState::windowSizeChanged,
                this, [this](double /* window_size */) {
                    _scene_dirty = true;
                    emit viewBoundsChanged();
                    update();
                });

        // Initial sync
        _scene_dirty = true;
    }
}

void HeatmapOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

std::pair<double, double> HeatmapOpenGLWidget::getViewBounds() const
{
    if (!_state) {
        return {-500.0, 500.0};  // Default bounds
    }

    double window_size = _state->getWindowSize();
    double half_window = window_size / 2.0;
    return {-half_window, half_window};
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void HeatmapOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Set clear color (dark theme)
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable multisampling if available
    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize the scene renderer
    if (!_scene_renderer.initialize()) {
        qWarning() << "HeatmapOpenGLWidget: Failed to initialize SceneRenderer";
        return;
    }

    _opengl_initialized = true;
}

void HeatmapOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_opengl_initialized) {
        return;
    }

    // Rebuild scene if needed
    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    // TODO: Render heatmap visualization here
    // For now, just clear the screen
}

void HeatmapOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);

    glViewport(0, 0, _widget_width, _widget_height);
    _scene_dirty = true;
    update();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void HeatmapOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    QOpenGLWidget::mousePressEvent(event);
}

void HeatmapOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseMoveEvent(event);
}

void HeatmapOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseReleaseEvent(event);
}

void HeatmapOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF world = screenToWorld(event->pos());
        emit plotDoubleClicked(static_cast<int64_t>(world.x()));
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void HeatmapOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    QOpenGLWidget::wheelEvent(event);
}

// =============================================================================
// Slots
// =============================================================================

void HeatmapOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

// =============================================================================
// Private Methods
// =============================================================================

void HeatmapOpenGLWidget::rebuildScene()
{
    if (!_state || !_data_manager) {
        _scene_renderer.clearScene();
        return;
    }

    // TODO: Gather trial-aligned AnalogTimeSeries data
    // TODO: Build heatmap visualization scene
    // For now, just clear the scene
    _scene_renderer.clearScene();
}

QPointF HeatmapOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    // TODO: Implement screen to world coordinate conversion
    // For now, return a placeholder
    return QPointF(0.0, 0.0);
}
