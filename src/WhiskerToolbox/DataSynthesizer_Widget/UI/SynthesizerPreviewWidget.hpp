/**
 * @file SynthesizerPreviewWidget.hpp
 * @brief OpenGL preview widget for visualizing synthesized AnalogTimeSeries
 *
 * Embeds a lightweight QOpenGLWidget that renders a single polyline from
 * a generated AnalogTimeSeries using the CorePlotting + PlottingOpenGL
 * rendering pipeline.
 */

#ifndef SYNTHESIZER_PREVIEW_WIDGET_HPP
#define SYNTHESIZER_PREVIEW_WIDGET_HPP

#include "PlottingOpenGL/SceneRenderer.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>

#include <memory>

class AnalogTimeSeries;

/**
 * @brief OpenGL widget for previewing synthesized AnalogTimeSeries data
 *
 * Renders a single polyline from an AnalogTimeSeries using an orthographic
 * projection fitted to the data bounds. Non-interactive — just displays
 * the waveform for visual verification before committing to DataManager.
 */
class SynthesizerPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit SynthesizerPreviewWidget(QWidget * parent = nullptr);
    ~SynthesizerPreviewWidget() override;

    SynthesizerPreviewWidget(SynthesizerPreviewWidget const &) = delete;
    SynthesizerPreviewWidget & operator=(SynthesizerPreviewWidget const &) = delete;
    SynthesizerPreviewWidget(SynthesizerPreviewWidget &&) = delete;
    SynthesizerPreviewWidget & operator=(SynthesizerPreviewWidget &&) = delete;

    /**
     * @brief Set a new AnalogTimeSeries for preview
     *
     * Rebuilds the scene from the series data and triggers a repaint.
     *
     * @param series The analog time series to visualize
     */
    void setData(std::shared_ptr<AnalogTimeSeries> series);

    /**
     * @brief Clear the preview display
     */
    void clearData();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    void rebuildScene();
    void updateProjection();

    PlottingOpenGL::SceneRenderer _scene_renderer;
    std::shared_ptr<AnalogTimeSeries> _series;
    bool _opengl_initialized{false};
    bool _scene_dirty{false};

    float _x_min{0.0f};
    float _x_max{1.0f};
    float _y_min{-1.0f};
    float _y_max{1.0f};

    glm::mat4 _projection_matrix{1.0f};
    glm::mat4 _view_matrix{1.0f};
};

#endif// SYNTHESIZER_PREVIEW_WIDGET_HPP
