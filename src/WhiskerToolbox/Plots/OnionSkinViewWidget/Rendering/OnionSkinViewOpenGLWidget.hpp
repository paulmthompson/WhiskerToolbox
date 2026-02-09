#ifndef ONION_SKIN_VIEW_OPENGL_WIDGET_HPP
#define ONION_SKIN_VIEW_OPENGL_WIDGET_HPP

/**
 * @file OnionSkinViewOpenGLWidget.hpp
 * @brief OpenGL-based onion skin view visualization widget
 *
 * Renders a temporal window of spatial data (PointData, LineData, MaskData)
 * around the current time position with alpha-graded fading. Elements at the
 * current time are fully opaque; elements further away fade based on the
 * configured alpha curve.
 *
 * Uses CorePlotting::SceneRenderer for all rendering. The scene is rebuilt
 * on each time position change, mapping points/lines/masks within the window
 * and assigning per-element alpha via computeTemporalAlpha().
 *
 * Per-glyph and per-line colors include the alpha channel, which the
 * GlyphRenderer and PolyLineRenderer pass through to the GPU. GL blending
 * is enabled (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA).
 *
 * @see OnionSkinViewState
 * @see SpatialMapper_Window.hpp
 * @see AlphaCurve.hpp
 * @see PlottingOpenGL::SceneRenderer
 */

#include "Core/OnionSkinViewState.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/Mappers/SpatialMapper_Window.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "Entity/EntityTypes.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

class DataManager;
class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering onion skin views
 *
 * Displays a temporal window of spatial data with alpha-graded fading.
 * Responds to time position changes to rebuild the scene. State holds
 * data keys, window parameters, alpha curve settings, and view transform.
 *
 * Features:
 * - Point rendering via SceneRenderer (GlyphRenderer) with per-glyph alpha
 * - Line rendering via SceneRenderer (PolyLineRenderer) with per-line alpha
 * - Mask contour rendering as polylines with per-line alpha
 * - Current-frame highlight (distinct color or enlarged size)
 * - Independent X/Y zooming, panning
 * - Temporal alpha: Linear, Exponential, or Gaussian falloff
 * - Depth-sorted rendering (farthest temporal distance drawn first)
 */
class OnionSkinViewOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit OnionSkinViewOpenGLWidget(QWidget * parent = nullptr);
    ~OnionSkinViewOpenGLWidget() override;

    OnionSkinViewOpenGLWidget(OnionSkinViewOpenGLWidget const &) = delete;
    OnionSkinViewOpenGLWidget & operator=(OnionSkinViewOpenGLWidget const &) = delete;
    OnionSkinViewOpenGLWidget(OnionSkinViewOpenGLWidget &&) = delete;
    OnionSkinViewOpenGLWidget & operator=(OnionSkinViewOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<OnionSkinViewState> state);
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Set the current time position
     *
     * Called when the time position changes (e.g., from scrubbing).
     * Triggers a scene rebuild with the new temporal window.
     *
     * @param time_index Current time frame index to center the window on
     */
    void setCurrentTime(int64_t time_index);

signals:
    void viewBoundsChanged();

    /**
     * @brief Emitted when an entity is selected via click
     * @param entity_id The selected entity's ID
     */
    void entitySelected(EntityId entity_id);

    /**
     * @brief Emitted on double-click to request frame jump to a specific entity
     * @param entity_id The entity to jump to
     */
    void entityDoubleClicked(EntityId entity_id);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private slots:
    void onStateChanged();
    void onViewStateChanged();
    void onDataKeysChanged();

private:
    std::shared_ptr<OnionSkinViewState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // --- Scene renderer ---
    PlottingOpenGL::SceneRenderer _scene_renderer;
    CorePlotting::RenderableScene _scene;

    bool _scene_dirty{true};
    bool _needs_bounds_update{true};  ///< True when bounds should be recalculated (data keys changed)
    bool _opengl_initialized{false};
    int64_t _current_time{0};

    int _widget_width{1};
    int _widget_height{1};

    CorePlotting::ViewStateData _cached_view_state;
    glm::mat4 _projection_matrix{1.0f};
    glm::mat4 _view_matrix{1.0f};

    bool _is_panning{false};
    QPoint _click_start_pos;
    QPoint _last_mouse_pos;
    static constexpr int DRAG_THRESHOLD = 4;

    void rebuildScene();
    void updateMatrices();
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes);
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;

    /**
     * @brief Brute-force nearest-point search on current frame's points only
     * @param world_pos Position in world coordinates
     * @param max_distance_sq Maximum squared distance for a hit (in world units)
     * @return EntityId of nearest point, or std::nullopt if none within range
     */
    [[nodiscard]] std::optional<EntityId> findNearestPointAtCurrentTime(
        QPointF const & world_pos, float max_distance_sq) const;

    /// Cached current-frame points (temporal_distance == 0), rebuilt each scene
    std::vector<CorePlotting::TimedMappedElement> _current_frame_points;
};

#endif  // ONION_SKIN_VIEW_OPENGL_WIDGET_HPP
