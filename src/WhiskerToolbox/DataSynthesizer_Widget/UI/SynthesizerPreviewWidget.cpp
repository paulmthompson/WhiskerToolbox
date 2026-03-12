/**
 * @file SynthesizerPreviewWidget.cpp
 * @brief Implementation of the OpenGL preview widget for synthesized data
 */

#include "SynthesizerPreviewWidget.hpp"

#include "CorePlotting/Layout/SeriesLayout.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Entity/EntityId.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <limits>
#include <numeric>

SynthesizerPreviewWidget::SynthesizerPreviewWidget(QWidget * parent)
    : QOpenGLWidget(parent) {
    QSurfaceFormat fmt;
    fmt.setVersion(4, 1);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSamples(4);
    setFormat(fmt);
}

SynthesizerPreviewWidget::~SynthesizerPreviewWidget() {
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void SynthesizerPreviewWidget::setData(std::shared_ptr<AnalogTimeSeries> series) {
    _series = std::move(series);
    _scene_dirty = true;
    update();
}

void SynthesizerPreviewWidget::clearData() {
    _series.reset();
    if (_opengl_initialized) {
        _scene_renderer.clearScene();
    }
    update();
}

void SynthesizerPreviewWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto const fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    if (!_scene_renderer.initialize()) {
        qWarning() << "SynthesizerPreviewWidget: Failed to initialize SceneRenderer";
        return;
    }

    _opengl_initialized = true;
}

void SynthesizerPreviewWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_opengl_initialized) {
        return;
    }

    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    _scene_renderer.render(_view_matrix, _projection_matrix);
}

void SynthesizerPreviewWidget::resizeGL(int w, int h) {
    glViewport(0, 0, std::max(1, w), std::max(1, h));
    updateProjection();
}

void SynthesizerPreviewWidget::rebuildScene() {
    if (!_series) {
        _scene_renderer.clearScene();
        return;
    }

    auto time_frame = _series->getTimeFrame();

    // The preview series is ephemeral (generated outside DataManager),
    // so it may not have a TimeFrame.  Create an identity one.
    if (!time_frame) {
        auto const n = static_cast<int>(_series->getNumSamples());
        std::vector<int> times(static_cast<std::size_t>(n));
        std::iota(times.begin(), times.end(), 0);
        time_frame = std::make_shared<TimeFrame>(times);
    }

    // Compute data bounds from the series
    float y_min = std::numeric_limits<float>::max();
    float y_max = std::numeric_limits<float>::lowest();
    float x_min = std::numeric_limits<float>::max();
    float x_max = std::numeric_limits<float>::lowest();

    for (auto const & tv: _series->view()) {
        float const val = tv.value();
        auto const x = static_cast<float>(time_frame->getTimeAtIndex(tv.time_frame_index));
        y_min = std::min(y_min, val);
        y_max = std::max(y_max, val);
        x_min = std::min(x_min, x);
        x_max = std::max(x_max, x);
    }

    // Handle degenerate cases
    float y_range = y_max - y_min;
    if (y_range < 1e-6f) {
        y_range = 1.0f;
    }
    float const x_range = x_max - x_min;
    if (x_range < 1e-6f) {
        x_max = x_min + 1.0f;
    }

    // Add 10% vertical padding
    _y_min = y_min - y_range * 0.1f;
    _y_max = y_max + y_range * 0.1f;
    _x_min = x_min;
    _x_max = x_max;

    updateProjection();

    // Build scene with identity layout (raw data coordinates)
    CorePlotting::SeriesLayout const layout;

    auto vertices = CorePlotting::TimeSeriesMapper::mapAnalogSeriesFull(
            *_series, layout, *time_frame, 1.0f);

    CorePlotting::PolyLineStyle style;
    style.color = glm::vec4(0.4f, 0.7f, 1.0f, 1.0f);
    style.thickness = 2.0f;

    CorePlotting::SceneBuilder builder;
    builder.setMatrices(_view_matrix, _projection_matrix)
            .addPolyLine("preview", std::move(vertices), EntityId(0), style);

    auto scene = builder.build();
    _scene_renderer.uploadScene(scene);
}

void SynthesizerPreviewWidget::updateProjection() {
    _projection_matrix = glm::ortho(_x_min, _x_max, _y_min, _y_max, -1.0f, 1.0f);
    _view_matrix = glm::mat4(1.0f);
}
