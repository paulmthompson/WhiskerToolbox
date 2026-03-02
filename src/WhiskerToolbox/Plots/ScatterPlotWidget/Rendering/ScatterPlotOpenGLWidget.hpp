#ifndef SCATTER_PLOT_OPENGL_WIDGET_HPP
#define SCATTER_PLOT_OPENGL_WIDGET_HPP

/**
 * @file ScatterPlotOpenGLWidget.hpp
 * @brief OpenGL-based scatter plot visualization widget
 *
 * Renders scatter points using SceneBuilder/SceneRenderer infrastructure.
 * Reads data from DataManager via ScatterPlotState data source configuration.
 * Supports pan and zoom; updates state on interaction and reads from state for projection.
 *
 * Selection modes:
 * - **SinglePoint**: Ctrl+Click to add/toggle a point to/from selection,
 *   Shift+Click to remove a point from selection.
 * - **Polygon**: Ctrl+Click to add vertices, Enter to close polygon and select
 *   enclosed points, Escape to cancel, Backspace to undo last vertex.
 *
 * @see ScatterPlotState
 * @see SceneRenderer for the rendering pipeline
 */

#include "Core/ScatterPointData.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <glm/glm.hpp>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <memory>
#include <optional>
#include <unordered_set>

class DataManager;
class GroupManager;
class GroupContextMenuHandler;
class QMenu;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class ScatterPlotState;

namespace CorePlotting {
    class RenderableScene;
    class SceneHitTester;
}

namespace CorePlotting::Interaction {
class PolygonInteractionController;
}

namespace PlottingOpenGL {
    class PreviewRenderer;
    class SceneRenderer;
}

/**
 * @brief OpenGL widget for rendering scatter plots
 *
 * Displays 2D scatter plots with pan/zoom; state holds view transform and axis ranges.
 * Uses SceneBuilder + SceneRenderer for glyph rendering with a y=x reference line.
 * Supports single-point and polygon selection modes.
 */
class ScatterPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit ScatterPlotOpenGLWidget(QWidget * parent = nullptr);
    ~ScatterPlotOpenGLWidget() override;

    ScatterPlotOpenGLWidget(ScatterPlotOpenGLWidget const &) = delete;
    ScatterPlotOpenGLWidget & operator=(ScatterPlotOpenGLWidget const &) = delete;
    ScatterPlotOpenGLWidget(ScatterPlotOpenGLWidget &&) = delete;
    ScatterPlotOpenGLWidget & operator=(ScatterPlotOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<ScatterPlotState> state);
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Set the GroupManager for group-related context menu actions
     * @param group_manager Pointer to the GroupManager (not owned)
     */
    void setGroupManager(GroupManager * group_manager);

signals:
    void viewBoundsChanged();
    void pointDoubleClicked(TimePosition position);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void keyPressEvent(QKeyEvent * event) override;
    void contextMenuEvent(QContextMenuEvent * event) override;

private slots:
    void onStateChanged();
    void onViewStateChanged();

private:
    std::shared_ptr<ScatterPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    int _widget_width{1};
    int _widget_height{1};

    CorePlotting::ViewStateData _cached_view_state;
    glm::mat4 _projection_matrix{1.0f};
    glm::mat4 _view_matrix{1.0f};

    bool _is_panning{false};
    bool _pan_eligible{false};///< True only when the press event intended a pan (no modifier selection)
    QPoint _click_start_pos;
    QPoint _last_mouse_pos;
    static constexpr int DRAG_THRESHOLD = 4;

    // Scene rendering
    std::unique_ptr<PlottingOpenGL::SceneRenderer> _scene_renderer;
    std::unique_ptr<CorePlotting::RenderableScene> _scene;
    bool _scene_dirty{true};
    bool _opengl_initialized{false};
    ScatterPointData _scatter_data;

    // Hit testing (double-click-to-navigate)
    std::unique_ptr<CorePlotting::SceneHitTester> _hit_tester;
    std::optional<std::size_t> _navigated_index;///< Index of last navigated-to point (for highlight)

    // Polygon interaction controller
    std::unique_ptr<CorePlotting::Interaction::PolygonInteractionController> _polygon_controller;
    std::unique_ptr<PlottingOpenGL::PreviewRenderer> _preview_renderer;
    std::vector<glm::vec2> _polygon_vertices_world;///< World-space vertices for containment test

    // Group context menu support
    GroupManager * _group_manager{nullptr};
    std::unique_ptr<GroupContextMenuHandler> _group_menu_handler;
    QMenu * _context_menu{nullptr};
    uint64_t _last_group_generation{0};///< Cached generation for change detection

    void updateMatrices();
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes);
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;
    void rebuildScene();

    /**
     * @brief Create the context menu and group handler
     */
    void createContextMenu();

    /**
     * @brief Hit test for a point at the given screen position
     * @param screen_pos The position in screen coordinates
     * @return Index into _scatter_data if a point was hit, nullopt otherwise
     */
    [[nodiscard]] std::optional<std::size_t> hitTestPointAt(QPoint const & screen_pos) const;

    /**
     * @brief Get the currently selected entities for the group context menu
     * @return Set of EntityIds corresponding to selected scatter points
     */
    [[nodiscard]] std::unordered_set<EntityId> getSelectedEntities() const;

    /**
     * @brief Get EntityId for a point at the given scatter data index
     * @param index Index into _scatter_data
     * @return EntityId if the point has an associated entity
     */
    [[nodiscard]] std::optional<EntityId> getEntityIdForPoint(std::size_t index) const;

    /**
     * @brief Apply group colors to the glyph batch after scene construction
     *
     * When color_by_group is enabled, iterates the scatter_points batch and
     * replaces each glyph's color with its group color from GroupManager.
     * Points not in any group retain the default glyph style color.
     */
    void applyGroupColorsToScene();

    // === Single-point selection ===
    void handleSinglePointCtrlClick(QPoint const & screen_pos);
    void handleSinglePointShiftClick(QPoint const & screen_pos);

    // === Polygon selection ===
    void handlePolygonCtrlClick(QMouseEvent * event);
    void completePolygonSelection();
    void cancelPolygonSelection();
};

#endif// SCATTER_PLOT_OPENGL_WIDGET_HPP
