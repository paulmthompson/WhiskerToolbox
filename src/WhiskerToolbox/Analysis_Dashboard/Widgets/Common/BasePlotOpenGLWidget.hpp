#pragma once

#include "CoreGeometry/boundingbox.hpp"
#include "Visualizers/RenderingContext.hpp"
#include "ViewState.hpp"
#include "Selection/SelectionModes.hpp"
#include "Selection/SelectionHandlers.hpp"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class GroupManager;
class PlotInteractionController;
class SelectionManager;
class TooltipManager;
class GenericViewAdapter;
struct BoundingBox;

/**
 * Base class for all plot widgets using Template Method pattern
 * Handles common OpenGL setup, interaction, and widget lifecycle
*/
class BasePlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit BasePlotOpenGLWidget(QWidget* parent = nullptr);
    virtual ~BasePlotOpenGLWidget();

    // Common interface
    void setGroupManager(GroupManager* group_manager);
    void setPointSize(float point_size);
    void setTooltipsEnabled(bool enabled);

    // View control
    virtual QVector2D screenToWorld(QPoint const& screen_pos) const;
    virtual QPoint worldToScreen(float world_x, float world_y) const;
    void resetView();

    // ViewState access for adapters
    ViewState& getViewState() { return _view_state; }
    const ViewState& getViewState() const { return _view_state; }

    // Selection management
    virtual void setSelectionMode(SelectionMode mode);
    void createSelectionHandler(SelectionMode mode);
    SelectionMode getSelectionMode() const { return _selection_mode; }
    void clearSelection();
    
    // Public event handlers for external access (e.g., event filters)
    virtual void handleKeyPress(QKeyEvent* event);

    float getPointSize() const { return _point_size; }
    float getLineWidth() const { return _line_width; }
    bool getTooltipsEnabled() const { return _tooltips_enabled; }

signals:
    void viewBoundsChanged(float left, float right, float bottom, float top);
    void mouseWorldMoved(float world_x, float world_y);
    void selectionModeChanged(SelectionMode mode);
    void selectionChanged(size_t total_selected, QString dataset_name, int point_index);
    void highlightStateChanged();
protected:
    // Template method pattern - defines the rendering algorithm
    void paintGL() final override;
    void initializeGL() override;
    void resizeGL(int w, int h) override;

    // Mouse events (delegates to interaction controller)
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;



    // Hooks for subclasses to implement
    virtual void renderData() = 0;
    virtual void calculateDataBounds() = 0;
    virtual BoundingBox getDataBounds() const = 0;
    virtual std::unique_ptr<SelectionManager> createSelectionManager() = 0;

    // Optional hooks
    virtual void renderBackground();
    virtual void renderOverlays();
    virtual void renderUI();
    virtual std::optional<QString> generateTooltipContent(QPoint const& screen_pos) const;

    // OpenGL context configuration (override in subclasses if needed)
    virtual std::pair<int, int> getRequiredOpenGLVersion() const { return {4, 1}; }
    virtual int getRequiredSamples() const { return 4; }

    // Helper methods
    RenderingContext createRenderingContext() const;
    void updateViewMatrices();
    void requestThrottledUpdate();

    // OpenGL context validation
    bool validateOpenGLContext() const;

protected:
    // Common state
    GroupManager* _group_manager = nullptr;
    float _point_size = 8.0f;
    float _line_width = 2.0f;
    bool _tooltips_enabled = true;
    bool _opengl_resources_initialized = false;

    // Selection mode
    SelectionMode _selection_mode = SelectionMode::None;
    SelectionVariant _selection_handler;
    std::function<void()> _selection_callback;

    // View state (encapsulates zoom, pan, bounds, etc.)
    ViewState _view_state;

    // View and projection matrices
    QMatrix4x4 _model_matrix;
    QMatrix4x4 _view_matrix;
    QMatrix4x4 _projection_matrix;

    // Shared services
    std::unique_ptr<PlotInteractionController> _interaction;
    std::unique_ptr<SelectionManager> _selection_manager;
    std::unique_ptr<TooltipManager> _tooltip_manager;

    // Update throttling
    QTimer* _fps_limiter_timer;
    bool _pending_update = false;

    friend GenericViewAdapter; // Allow GenericViewAdapter to access private members

private:
    bool initializeRendering();
    void computeCameraWorldView(float& center_x, float& center_y, 
                               float& world_width, float& world_height) const;
};
