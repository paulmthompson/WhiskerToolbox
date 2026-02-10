#ifndef LINEPLOT_OPENGLWIDGET_HPP
#define LINEPLOT_OPENGLWIDGET_HPP

/**
 * @file LinePlotOpenGLWidget.hpp
 * @brief OpenGL-based line plot visualization with batch line rendering & selection
 * 
 * This widget renders AnalogTimeSeries data as line plots,
 * aligned to trial intervals specified via LinePlotState.
 * 
 * Architecture:
 * - Receives LinePlotState for alignment, view settings, and line options
 * - Uses GatherResult<AnalogTimeSeries> for trial-aligned data
 * - Uses CorePlotting::LineBatchBuilder to create LineBatchData from gathered trials
 * - Uses PlottingOpenGL::BatchLineStore + BatchLineRenderer for GPU rendering
 * - Uses ILineBatchIntersector (GPU compute or CPU fallback) for line selection
 * 
 * Selection:
 * - Ctrl+Click to start drawing a selection line
 * - Drag to extend the selection line
 * - Release to complete — intersected trial lines are selected
 * - Shift+Ctrl+Click for remove mode (deselect intersected trials)
 * - Emits trialsSelected() with 0-based trial indices
 * 
 * @see CorePlotting/LineBatch/LineBatchBuilder.hpp
 * @see PlottingOpenGL/LineBatch/BatchLineRenderer.hpp
 */

#include "Core/LinePlotState.hpp"

#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "CorePlotting/LineBatch/ILineBatchIntersector.hpp"
#include "CorePlotting/LineBatch/LineBatchData.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "PlottingOpenGL/LineBatch/BatchLineStore.hpp"
#include "PlottingOpenGL/LineBatch/BatchLineRenderer.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DataManager/utils/GatherResult.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class DataManager;
class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering line plots with batch line selection
 * 
 * Displays AnalogTimeSeries data aligned to trial intervals. Each trial
 * is shown as a line plot with values rendered at their relative time positions.
 * 
 * Features:
 * - Independent X (time) and Y (value) zooming
 * - Panning with mouse drag
 * - Wheel zoom (Shift+wheel for Y-only)
 * - Line selection via Ctrl+Click drag (intersects trials)
 * - Selection result emitted as trial indices
 */
class LinePlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit LinePlotOpenGLWidget(QWidget * parent = nullptr);
    ~LinePlotOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    LinePlotOpenGLWidget(LinePlotOpenGLWidget const &) = delete;
    LinePlotOpenGLWidget & operator=(LinePlotOpenGLWidget const &) = delete;
    LinePlotOpenGLWidget(LinePlotOpenGLWidget &&) = delete;
    LinePlotOpenGLWidget & operator=(LinePlotOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<LinePlotState> state);
    void setDataManager(std::shared_ptr<DataManager> data_manager);
    [[nodiscard]] std::pair<double, double> getViewBounds() const;

    /// Currently selected trial indices (0-based into GatherResult)
    [[nodiscard]] std::vector<std::uint32_t> const & selectedTrialIndices() const { return _selected_trial_indices; }

    /// Clear all selected trials
    void clearSelection();

signals:
    /// Emitted on double-click with absolute time and the data series key.
    /// The absolute time accounts for alignment offset (relative → absolute).
    void plotDoubleClicked(int64_t absolute_time, QString series_key);
    void viewBoundsChanged();

    /// Emitted when line selection changes. Indices are 0-based into GatherResult.
    void trialsSelected(std::vector<std::uint32_t> const & trial_indices);

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
    void onWindowSizeChanged(double window_size);

private:
    std::shared_ptr<LinePlotState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // --- Scene renderer (for future axes, grids, etc.) ---
    PlottingOpenGL::SceneRenderer _scene_renderer;
    CorePlotting::RenderableScene _scene;

    // --- Batch line rendering (trials as lines) ---
    PlottingOpenGL::BatchLineStore _line_store;
    PlottingOpenGL::BatchLineRenderer _line_renderer{_line_store};
    std::unique_ptr<CorePlotting::ILineBatchIntersector> _intersector;

    bool _scene_dirty{true};
    bool _opengl_initialized{false};

    CorePlotting::ViewStateData _cached_view_state;
    glm::mat4 _view_matrix{1.0f};
    glm::mat4 _projection_matrix{1.0f};

    // --- Panning state ---
    bool _is_panning{false};
    QPoint _click_start_pos;
    QPoint _last_mouse_pos;
    static constexpr int DRAG_THRESHOLD = 4;

    // --- Line selection state ---
    bool _is_selecting{false};
    glm::vec2 _selection_start_ndc{0.0f};
    glm::vec2 _selection_end_ndc{0.0f};
    QPoint _selection_start_screen{0, 0};
    QPoint _selection_end_screen{0, 0};
    bool _selection_remove_mode{false};
    std::vector<std::uint32_t> _selected_trial_indices;

    // --- Cached alignment data (for relative → absolute time conversion) ---
    std::vector<std::int64_t> _cached_alignment_times;
    std::string _cached_series_key;

    int _widget_width{1};
    int _widget_height{1};

    void rebuildScene();
    void updateMatrices();
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;
    [[nodiscard]] glm::vec2 screenToNDC(QPoint const & screen_pos) const;
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes);
    [[nodiscard]] GatherResult<AnalogTimeSeries> gatherTrialData() const;

    // --- Selection helpers ---
    void startSelection(QPoint const & screen_pos, bool remove_mode);
    void updateSelection(QPoint const & screen_pos);
    void completeSelection();
    void cancelSelection();
    [[nodiscard]] CorePlotting::Interaction::GlyphPreview buildSelectionPreview() const;
    void applyIntersectionResults(std::vector<CorePlotting::LineBatchIndex> const & hit_indices,
                                  bool remove);
};

#endif // LINEPLOT_OPENGLWIDGET_HPP
