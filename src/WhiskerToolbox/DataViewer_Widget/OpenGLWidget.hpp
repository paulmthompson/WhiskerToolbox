#ifndef OPENGLWIDGET_HPP
#define OPENGLWIDGET_HPP

#include "XAxis.hpp"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>

#include <map>
#include <memory>
#include <string>
#include <vector>


class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;

struct AnalogSeriesData {
    std::shared_ptr<AnalogTimeSeries> series;
    std::string color;
    std::shared_ptr<TimeFrame> time_frame;
};

struct DigitalEventSeriesData {
    std::shared_ptr<DigitalEventSeries> series;
    std::string color;
    std::shared_ptr<TimeFrame> time_frame;
};

struct DigitalIntervalSeriesData {
    std::shared_ptr<DigitalIntervalSeries> series;
    std::string color;
    std::shared_ptr<TimeFrame> time_frame;
};

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    OpenGLWidget(QWidget *parent = nullptr);

    virtual ~OpenGLWidget();

    void addAnalogTimeSeries(
            std::string key,
            std::shared_ptr<AnalogTimeSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string color = "");
    void removeAnalogTimeSeries(const std::string &key);
    void addDigitalEventSeries(
            std::string key,
            std::shared_ptr<DigitalEventSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string color = "");
    void removeDigitalEventSeries(const std::string &key);
    void addDigitalIntervalSeries(
            std::string key,
            std::shared_ptr<DigitalIntervalSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string color = "");
    void removeDigitalIntervalSeries(const std::string &key);
    void clearSeries();
    void setBackgroundColor(const std::string &hexColor);
    void setXLimit(int xmax) {_xAxis.setMax(xmax); };
    void changeZoom(int zoom)
    {
        int center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;

        zoom = (_xAxis.getEnd() - _xAxis.getStart()) - zoom;
       _xAxis.setCenterAndZoom(center, zoom);
        updateCanvas(_time);
    }

    XAxis getXAxis() const {return _xAxis;}

public slots:
    void updateCanvas(int time);

protected:
    void initializeGL() override;
    void paintGL() override;

    void resizeGL(int w, int h) override;

    void cleanup();

private:
    void setupVertexAttribs();
    void generateRandomValues(int count);
    void generateAndAddFakeData(int count);
    void adjustFakeData();
    void drawDigitalEventSeries();
    void drawDigitalIntervalSeries();
    void drawAnalogSeries();
    void drawAxis();
    void drawGridLines();
    void drawDashedLine(float xStart, float xEnd, float yStart, float yEnd, int dashLength, int gapLength);

    std::map<std::string, AnalogSeriesData> _analog_series;
    std::map<std::string, DigitalEventSeriesData> _digital_event_series;
    std::map<std::string, DigitalIntervalSeriesData> _digital_interval_series;

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

    QOpenGLShaderProgram *m_dashedProgram {0};
    int m_dashedProjMatrixLoc;
    int m_dashedViewMatrixLoc;
    int m_dashedModelMatrixLoc;
    int m_dashedResolutionLoc;
    int m_dashedDashSizeLoc;
    int m_dashedGapSizeLoc;

    std::string m_background_color {"#000000"}; // black

    std::vector<GLfloat> m_vertices; // for testing
};




#endif //OPENGLWIDGET_HPP
