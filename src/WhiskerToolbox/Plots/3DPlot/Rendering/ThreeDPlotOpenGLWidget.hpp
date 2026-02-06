#ifndef THREE_D_PLOT_OPENGL_WIDGET_HPP
#define THREE_D_PLOT_OPENGL_WIDGET_HPP

/**
 * @file ThreeDPlotOpenGLWidget.hpp
 * @brief OpenGL-based 3D plot visualization widget
 * 
 * This widget renders 3D visualizations for navigating in a 3D arena.
 * Supports zoom, pan, and rotate camera controls.
 * 
 * Architecture:
 * - Receives ThreeDPlotState for camera settings and plot options
 * - Uses OpenGL for efficient 3D rendering
 * 
 * @see ThreeDPlotState
 */

#include "Core/3DPlotState.hpp"

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering 3D plots
 * 
 * Displays 3D visualizations with camera controls for zoom, pan, and rotate.
 */
class ThreeDPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit ThreeDPlotOpenGLWidget(QWidget * parent = nullptr);
    ~ThreeDPlotOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    ThreeDPlotOpenGLWidget(ThreeDPlotOpenGLWidget const &) = delete;
    ThreeDPlotOpenGLWidget & operator=(ThreeDPlotOpenGLWidget const &) = delete;
    ThreeDPlotOpenGLWidget(ThreeDPlotOpenGLWidget &&) = delete;
    ThreeDPlotOpenGLWidget & operator=(ThreeDPlotOpenGLWidget &&) = delete;

    /**
     * @brief Set the ThreeDPlotState for this widget
     * 
     * The state provides camera settings and plot options.
     * The widget connects to state signals to react to changes.
     * 
     * @param state Shared pointer to ThreeDPlotState
     */
    void setState(std::shared_ptr<ThreeDPlotState> state);

    /**
     * @brief Update the plot with new time position and data
     * 
     * Called when time changes to update the visualization with
     * data from all added keys at the given time position.
     * 
     * @param data_manager Shared pointer to DataManager for data access
     * @param position The current TimePosition
     */
    void updateTime(std::shared_ptr<DataManager> data_manager,
                    TimePosition position);

signals:
    /**
     * @brief Emitted when view bounds change (camera position/orientation changes)
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
    void wheelEvent(QWheelEvent * event) override;

private slots:
    /**
     * @brief Update rendering when state changes
     */
    void onStateChanged();

private:
    /**
     * @brief Initialize shader program for 3D point rendering
     */
    void _initializeShaders();

    /**
     * @brief Initialize vertex buffers and arrays
     */
    void _initializeBuffers();

    /**
     * @brief Initialize grid buffers
     */
    void _initializeGridBuffers();

    /**
     * @brief Update projection matrix based on widget size
     */
    void _updateProjectionMatrix();

    /**
     * @brief Update view matrix based on camera state
     */
    void _updateViewMatrix();

    /**
     * @brief Render the grid at z=0
     */
    void _renderGrid();

    // State management
    std::shared_ptr<ThreeDPlotState> _state;

    // Cached data for reloading when keys change
    std::shared_ptr<DataManager> _last_data_manager;
    TimePosition _last_time_position;

    // Widget dimensions
    int _widget_width{1};
    int _widget_height{1};

    // OpenGL resources for points
    QOpenGLShaderProgram * _shader_program{nullptr};
    QOpenGLVertexArrayObject _vao;
    QOpenGLBuffer _vbo{QOpenGLBuffer::VertexBuffer};
    std::vector<float> _point_data;  // 3D positions (x, y, z) as float array
    int _point_count{0};

    // OpenGL resources for grid
    QOpenGLShaderProgram * _grid_shader_program{nullptr};
    QOpenGLVertexArrayObject _grid_vao;
    QOpenGLBuffer _grid_vbo{QOpenGLBuffer::VertexBuffer};
    std::vector<float> _grid_data;  // Grid line vertices
    int _grid_vertex_count{0};

    // Matrices
    QMatrix4x4 _projection_matrix;
    QMatrix4x4 _view_matrix;

    // Camera state
    float _camera_distance{500.0f};      // Distance from origin
    float _camera_azimuth{0.0f};        // Rotation around Y axis (degrees)
    float _camera_elevation{30.0f};     // Rotation around X axis (degrees)
    QVector3D _camera_pan{0.0f, 0.0f, 0.0f};  // Pan offset in XY plane

    // Mouse interaction state
    QPoint _last_mouse_pos;
    bool _is_rotating{false};
    bool _is_panning{false};
    Qt::MouseButton _active_button{Qt::NoButton};
};

#endif // THREE_D_PLOT_OPENGL_WIDGET_HPP
