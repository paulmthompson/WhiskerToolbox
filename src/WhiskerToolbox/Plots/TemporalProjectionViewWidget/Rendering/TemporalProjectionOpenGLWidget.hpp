#ifndef TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP
#define TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP

/**
 * @file TemporalProjectionOpenGLWidget.hpp
 * @brief OpenGL-based temporal projection view visualization with SceneRenderer
 *
 * Renders all spatial data (PointData, LineData) across all time frames in a
 * single spatial overlay view. Uses CorePlotting::SceneRenderer for points
 * and PlottingOpenGL::BatchLineRenderer for selectable lines (SSBO-based).
 *
 * Architecture:
 * - SceneRenderer handles point glyphs (via GlyphRenderer)
 * - BatchLineStore + BatchLineRenderer handle lines with selection support
 * - Uses SpatialMapper_AllTimes to flatten all time frames
 * - Supports pan/zoom via PlotInteractionHelpers
 *
 * @see TemporalProjectionViewState
 * @see PlottingOpenGL::SceneRenderer
 * @see PlottingOpenGL::BatchLineRenderer
 */

#include "Core/TemporalProjectionViewState.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "CorePlotting/LineBatch/ILineBatchIntersector.hpp"
#include "CorePlotting/LineBatch/LineBatchData.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "PlottingOpenGL/LineBatch/BatchLineRenderer.hpp"
#include "PlottingOpenGL/LineBatch/BatchLineStore.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"
#include "Entity/EntityTypes.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>
#include <unordered_set>
#include <vector>

class DataManager;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

/**
 * @brief OpenGL widget for rendering temporal projection views
 *
 * Displays all spatial data across all time frames with pan/zoom.
 * State holds data keys, view transform, rendering params.
 *
 * Features:
 * - Point rendering via SceneRenderer (GlyphRenderer)
 * - Line rendering via BatchLineStore/BatchLineRenderer (SSBO selection support)
 * - Independent X/Y zooming, panning
 * - Wheel zoom (Shift+wheel for Y-only, Ctrl+wheel for both axes)
 */
class TemporalProjectionOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit TemporalProjectionOpenGLWidget(QWidget * parent = nullptr);
    ~TemporalProjectionOpenGLWidget() override;

    TemporalProjectionOpenGLWidget(TemporalProjectionOpenGLWidget const &) = delete;
    TemporalProjectionOpenGLWidget & operator=(TemporalProjectionOpenGLWidget const &) = delete;
    TemporalProjectionOpenGLWidget(TemporalProjectionOpenGLWidget &&) = delete;
    TemporalProjectionOpenGLWidget & operator=(TemporalProjectionOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<TemporalProjectionViewState> state);
    void setDataManager(std::shared_ptr<DataManager> data_manager);
    
    [[nodiscard]] std::pair<double, double> getViewBounds() const;

    /// Currently selected entity IDs (from points or lines)
    [[nodiscard]] std::unordered_set<EntityId> const & selectedEntityIds() const { return _selected_entity_ids; }

    /// Clear all selections
    void clearSelection();

signals:
    void viewBoundsChanged();

    /// Emitted when selection changes (point or line selection)
    void entitiesSelected(std::unordered_set<EntityId> const & entity_ids);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void keyReleaseEvent(QKeyEvent * event) override;

private slots:
    void onStateChanged();
    void onViewStateChanged();
    void onDataKeysChanged();

private:
    std::shared_ptr<TemporalProjectionViewState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // --- Scene renderer (points, lines, future axes/grids) ---
    PlottingOpenGL::SceneRenderer _scene_renderer;
    CorePlotting::RenderableScene _scene;

    // --- Batch line rendering for selectable lines ---
    PlottingOpenGL::BatchLineStore _line_store;
    PlottingOpenGL::BatchLineRenderer _line_renderer{_line_store};
    std::unique_ptr<CorePlotting::ILineBatchIntersector> _intersector;

    bool _scene_dirty{true};
    bool _opengl_initialized{false};

    int _widget_width{1};
    int _widget_height{1};

    CorePlotting::ViewStateData _cached_view_state;
    glm::mat4 _projection_matrix{1.0f};
    glm::mat4 _view_matrix{1.0f};

    bool _is_panning{false};
    QPoint _click_start_pos;
    QPoint _last_mouse_pos;
    static constexpr int DRAG_THRESHOLD = 4;

    // --- Selection state ---
    bool _is_selecting{false};
    glm::vec2 _selection_start_ndc{0.0f};
    glm::vec2 _selection_end_ndc{0.0f};
    QPoint _selection_start_screen{0, 0};
    QPoint _selection_end_screen{0, 0};
    bool _selection_remove_mode{false};
    std::unordered_set<EntityId> _selected_entity_ids;

    void rebuildScene();
    void updateMatrices();
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes);
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;
    [[nodiscard]] glm::vec2 screenToNDC(QPoint const & screen_pos) const;

    // --- Selection helpers ---
    void handleClickSelection(QPoint const & screen_pos);
    void startLineSelection(QPoint const & screen_pos, bool remove_mode);
    void updateLineSelection(QPoint const & screen_pos);
    void completeLineSelection();
    void cancelLineSelection();
    [[nodiscard]] CorePlotting::Interaction::GlyphPreview buildSelectionPreview() const;
    void applyLineIntersectionResults(std::vector<CorePlotting::LineBatchIndex> const & hit_indices, bool remove);
};

#endif  // TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP
