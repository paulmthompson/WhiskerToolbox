#include "ScatterPlotOpenGLWidget.hpp"

#include "Core/ScatterPlotState.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
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

ScatterPlotOpenGLWidget::~ScatterPlotOpenGLWidget()
{
    makeCurrent();
    doneCurrent();
}

void ScatterPlotOpenGLWidget::setState(std::shared_ptr<ScatterPlotState> state)
{
    // Disconnect old state signals
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        // Connect to state signals
        connect(_state.get(), &ScatterPlotState::stateChanged,
                this, &ScatterPlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &ScatterPlotState::xMinChanged,
                this, [this](double /* x_min */) {
                    emit viewBoundsChanged();
                    update();
                });
        connect(_state.get(), &ScatterPlotState::xMaxChanged,
                this, [this](double /* x_max */) {
                    emit viewBoundsChanged();
                    update();
                });
        connect(_state.get(), &ScatterPlotState::yMinChanged,
                this, [this](double /* y_min */) {
                    emit viewBoundsChanged();
                    update();
                });
        connect(_state.get(), &ScatterPlotState::yMaxChanged,
                this, [this](double /* y_max */) {
                    emit viewBoundsChanged();
                    update();
                });

        // Initial update
        update();
    }
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void ScatterPlotOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Set clear color (dark theme)
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ScatterPlotOpenGLWidget::paintGL()
{
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // For now, just render a blank dark background
    // TODO: Add scatter plot rendering logic here
}

void ScatterPlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = w;
    _widget_height = h;

    glViewport(0, 0, w, h);
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void ScatterPlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    QOpenGLWidget::mousePressEvent(event);
    // TODO: Implement mouse interaction
}

void ScatterPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseMoveEvent(event);
    // TODO: Implement mouse interaction
}

void ScatterPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseReleaseEvent(event);
    // TODO: Implement mouse interaction
}

void ScatterPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseDoubleClickEvent(event);
    // TODO: Implement mouse interaction
}

void ScatterPlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    QOpenGLWidget::wheelEvent(event);
    // TODO: Implement zoom interaction
}

// =============================================================================
// Private Slots
// =============================================================================

void ScatterPlotOpenGLWidget::onStateChanged()
{
    update();
}
