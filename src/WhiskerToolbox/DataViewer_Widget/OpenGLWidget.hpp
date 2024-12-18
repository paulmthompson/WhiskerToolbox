#ifndef OPENGLWIDGET_HPP
#define OPENGLWIDGET_HPP

#include "XAxis.hpp"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>

#include <memory>
#include <string>
#include <vector>


class AnalogTimeSeries;
class QWheelEvent;

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    OpenGLWidget(QWidget *parent = nullptr);

    virtual ~OpenGLWidget();

    void addAnalogTimeSeries(std::shared_ptr<AnalogTimeSeries> series, std::string color = "");
    void clearSeries();
    void setBackgroundColor(const std::string &hexColor);
    void setXLimit(int xmax) {_xAxis.setMax(xmax); };

public slots:
    void updateCanvas(int time);

protected:
    void initializeGL() override;
    void paintGL() override;

    void resizeGL(int w, int h) override;
    void wheelEvent(QWheelEvent *event) override;

    void cleanup();

private:
    void setupVertexAttribs();
    void generateRandomValues(int count);
    void generateAndAddFakeData(int count);
    void adjustFakeData();

    std::vector<std::shared_ptr<AnalogTimeSeries>> _analog_series;
    std::vector<std::pair<float, float>> _series_min_max;
    std::vector<std::string> _series_colors;

    XAxis _xAxis;
    int _time {0};

    QOpenGLShaderProgram *m_program {0};
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;
    QMatrix4x4 m_proj;
    QMatrix4x4 m_view;
    QMatrix4x4 m_model;
    int m_projMatrixLoc;
    int m_viewMatrixLoc;
    int m_modelMatrixLoc;

    std::string m_background_color {"#000000"}; // black

    std::vector<GLfloat> m_vertices; // for testing
};

std::string generateRandomColor();


#endif //OPENGLWIDGET_HPP
