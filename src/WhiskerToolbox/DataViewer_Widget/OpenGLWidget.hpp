#ifndef OPENGLWIDGET_HPP
#define OPENGLWIDGET_HPP

#include "XAxis.hpp"
#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
//#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;
class QMouseEvent;

struct AnalogSeriesData {
    std::shared_ptr<AnalogTimeSeries> series;
    std::shared_ptr<TimeFrame> time_frame;
    std::unique_ptr<AnalogTimeSeriesDisplayOptions> display_options;
};

struct DigitalEventSeriesData {
    std::shared_ptr<DigitalEventSeries> series;
    std::shared_ptr<TimeFrame> time_frame;
    std::unique_ptr<DigitalEventSeriesDisplayOptions> display_options;
};

struct DigitalIntervalSeriesData {
    std::shared_ptr<DigitalIntervalSeries> series;
    std::shared_ptr<TimeFrame> time_frame;
    std::unique_ptr<DigitalIntervalSeriesDisplayOptions> display_options;
};

struct LineParameters {
    float xStart = 0.0f;
    float xEnd = 0.0f;
    float yStart = 0.0f;
    float yEnd = 0.0f;
    float dashLength = 5.0f;
    float gapLength = 5.0f;
};

enum class PlotTheme {
    Dark,   // Black background, white axes (default)
    Light   // White background, dark axes
};

//class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit OpenGLWidget(QWidget * parent = nullptr);

    ~OpenGLWidget() override;

    void addAnalogTimeSeries(
            std::string const & key,
            std::shared_ptr<AnalogTimeSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string const & color = "");
    void removeAnalogTimeSeries(std::string const & key);
    void addDigitalEventSeries(
            std::string const & key,
            std::shared_ptr<DigitalEventSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string const & color = "");
    void removeDigitalEventSeries(std::string const & key);
    void addDigitalIntervalSeries(
            std::string const & key,
            std::shared_ptr<DigitalIntervalSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string const & color = "");
    void removeDigitalIntervalSeries(std::string const & key);
    void clearSeries();
    void setBackgroundColor(std::string const & hexColor);

    void setPlotTheme(PlotTheme theme);
    [[nodiscard]] PlotTheme getPlotTheme() const { return _plot_theme; }

    // Grid line controls
    void setGridLinesEnabled(bool enabled) { _grid_lines_enabled = enabled; updateCanvas(_time); }
    [[nodiscard]] bool getGridLinesEnabled() const { return _grid_lines_enabled; }
    void setGridSpacing(int spacing) { _grid_spacing = spacing; updateCanvas(_time); }
    [[nodiscard]] int getGridSpacing() const { return _grid_spacing; }

    // Vertical spacing controls for analog series
    void setVerticalSpacing(float spacing) { _ySpacing = spacing; updateCanvas(_time); }
    [[nodiscard]] float getVerticalSpacing() const { return _ySpacing; }

    // Interval selection controls
    void setSelectedInterval(std::string const & series_key, int64_t start_time, int64_t end_time);
    void clearSelectedInterval(std::string const & series_key);
    std::optional<std::pair<int64_t, int64_t>> getSelectedInterval(std::string const & series_key) const;
    std::optional<std::pair<int64_t, int64_t>> findIntervalAtTime(std::string const & series_key, float time_coord) const;

    // Interval edge dragging controls
    std::optional<std::pair<std::string, bool>> findIntervalEdgeAtPosition(float canvas_x, float canvas_y) const;
    void startIntervalDrag(std::string const & series_key, bool is_left_edge, QPoint const & start_pos);
    void updateIntervalDrag(QPoint const & current_pos);
    void finishIntervalDrag();
    void cancelIntervalDrag();

    [[nodiscard]] bool isDraggingInterval() const { return _is_dragging_interval; }

    void setXLimit(int xmax) {
        _xAxis.setMax(xmax);
    };

    void changeRangeWidth(int64_t range_delta) {
        int64_t const center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;
        int64_t const current_range = _xAxis.getEnd() - _xAxis.getStart();
        int64_t const new_range = current_range + range_delta; // Add delta to current range
        _xAxis.setCenterAndZoom(center, new_range);
        updateCanvas(_time);
    }

    int64_t setRangeWidth(int64_t range_width) {
        int64_t const center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;
        int64_t const actual_range = _xAxis.setCenterAndZoomWithFeedback(center, range_width);
        updateCanvas(_time);
        return actual_range; // Return the actual range width achieved
    }

    [[nodiscard]] XAxis getXAxis() const { return _xAxis; }

    void setGlobalScale(float scale) {
        _global_zoom = scale;
        updateCanvas(_time);
    }

    [[nodiscard]] std::pair<int, int> getCanvasSize() const {
        return std::make_pair(width(), height());
    }

    // Coordinate conversion methods
    [[nodiscard]] float canvasXToTime(float canvas_x) const;
    [[nodiscard]] float canvasYToAnalogValue(float canvas_y, std::string const & series_key) const;

    // Display options getters (similar to Media_Window pattern)
    [[nodiscard]] std::optional<AnalogTimeSeriesDisplayOptions *> getAnalogConfig(std::string const & key) const {
        auto it = _analog_series.find(key);
        if (it != _analog_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<DigitalEventSeriesDisplayOptions *> getDigitalEventConfig(std::string const & key) const {
        auto it = _digital_event_series.find(key);
        if (it != _digital_event_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<DigitalIntervalSeriesDisplayOptions *> getDigitalIntervalConfig(std::string const & key) const {
        auto it = _digital_interval_series.find(key);
        if (it != _digital_interval_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

public slots:
    void updateCanvas(int time);

signals:
    void mouseHover(float time_coordinate, float canvas_y, QString const & series_info);
    void mouseClick(float time_coordinate, float canvas_y, QString const & series_info);

protected:
    void initializeGL() override;
    void paintGL() override;

    void resizeGL(int w, int h) override;

    void cleanup();

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;

private:
    void setupVertexAttribs();
    //void generateRandomValues(int count);
    //void generateAndAddFakeData(int count);
    //void adjustFakeData();
    void drawDigitalEventSeries();
    void drawDigitalIntervalSeries();
    void drawAnalogSeries();
    void drawAxis();
    void drawGridLines();
    void drawDashedLine(LineParameters const & params);
    void drawDraggedInterval();
    void _addSeries(std::string const & key);
    void _removeSeries(std::string const & key);
    void _updateYViewBoundaries();
    
    // Gap detection helper methods for analog series
    template<typename Iterator>
    void _drawAnalogSeriesWithGapDetection(Iterator start_it, Iterator end_it,
                                          std::vector<float> const & data,
                                          std::vector<size_t> const & data_time,
                                          std::shared_ptr<TimeFrame> const & time_frame,
                                          float gap_threshold,
                                          float rNorm, float gNorm, float bNorm);
    
    template<typename Iterator>
    void _drawAnalogSeriesAsMarkers(Iterator start_it, Iterator end_it,
                                   std::vector<float> const & data,
                                   std::vector<size_t> const & data_time,
                                   std::shared_ptr<TimeFrame> const & time_frame,
                                   float rNorm, float gNorm, float bNorm);

    // Gap analysis for automatic display mode selection
    struct GapAnalysis {
        bool has_gaps{false};
        int gap_count{0};
        float max_gap_size{0.0f};
        float recommended_threshold{5.0f};
    };
    
    GapAnalysis _analyzeDataGaps(AnalogTimeSeries const & series, TimeFrame const & time_frame);

    std::unordered_map<std::string, AnalogSeriesData> _analog_series;
    std::unordered_map<std::string, DigitalEventSeriesData> _digital_event_series;
    std::unordered_map<std::string, DigitalIntervalSeriesData> _digital_interval_series;
    std::unordered_map<std::string, int> _series_y_position;

    XAxis _xAxis;
    int _time{0};

    QOpenGLShaderProgram * m_program{nullptr};
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;
    QMatrix4x4 m_proj; // Initialized as identity
    QMatrix4x4 m_view; // Initialized as identity
    QMatrix4x4 m_model;// Initialized as identity
    int m_projMatrixLoc{};
    int m_viewMatrixLoc{};
    int m_modelMatrixLoc{};

    QOpenGLShaderProgram * m_dashedProgram{nullptr};
    int m_dashedProjMatrixLoc{};
    int m_dashedViewMatrixLoc{};
    int m_dashedModelMatrixLoc{};
    int m_dashedResolutionLoc{};
    int m_dashedDashSizeLoc{};
    int m_dashedGapSizeLoc{};

    float _global_zoom{1.0f};
    float _verticalPanOffset{0.0f};
    QPoint _lastMousePos{};
    bool _isPanning{false};
    float _yMin{-1.0f};
    float _yMax{1.0f};
    float _ySpacing{0.1f};

    std::string m_background_color{"#000000"};// black
    std::string m_axis_color{"#FFFFFF"};// white (for dark theme)

    std::vector<GLfloat> m_vertices;// for testing

    PlotTheme _plot_theme{PlotTheme::Dark};

    // Grid line settings
    bool _grid_lines_enabled{false}; // Default to disabled
    int _grid_spacing{100}; // Default spacing of 100 time units

    // Interval selection tracking
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> _selected_intervals;

    // Interval dragging state
    bool _is_dragging_interval{false};
    std::string _dragging_series_key;
    bool _dragging_left_edge{false}; // true for left edge, false for right edge
    int64_t _original_start_time{0};
    int64_t _original_end_time{0};
    int64_t _dragged_start_time{0};
    int64_t _dragged_end_time{0};
    QPoint _drag_start_pos;
};


#endif//OPENGLWIDGET_HPP
