/**
 * @file MultiLaneVerticalAxisWidget.hpp
 * @brief Widget for rendering a multi-lane vertical axis with per-lane labels
 *
 * Displays channel/series names alongside each lane in the DataViewer,
 * with horizontal separator lines between lanes. Unlike the single-range
 * VerticalAxisWidget, this widget manages independent lanes that each have
 * their own Y position and extent.
 *
 * Viewport-aware: only renders labels for lanes visible in the current
 * Y viewport (respects y_pan from ViewStateData).
 *
 * Supports interactive lane drag-and-drop reordering. Clicking and dragging
 * a lane label emits laneReorderRequested(source_lane_id, target_visual_slot)
 * where target_visual_slot is 0-based from the top of the widget. Escape
 * cancels an in-progress drag.
 */

#ifndef MULTI_LANE_VERTICAL_AXIS_WIDGET_HPP
#define MULTI_LANE_VERTICAL_AXIS_WIDGET_HPP

#include <QString>
#include <QWidget>

#include <functional>

class MultiLaneVerticalAxisState;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

/**
 * @brief Widget that renders per-lane labels and separators for a stacked layout
 *
 * Shows:
 * - Channel name labels centered vertically in each lane
 * - Horizontal separator lines at lane boundaries
 * - Viewport-aware rendering (culls off-screen lanes)
 *
 * The widget is driven by MultiLaneVerticalAxisState, which receives
 * lane descriptors from the DataViewer after each layout computation.
 */
class MultiLaneVerticalAxisWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Function type that returns the effective Y viewport bounds
     *
     * Returns {y_min, y_max} in world coordinates after applying zoom and pan.
     * Used to keep labels aligned with the OpenGL rendering surface.
     */
    using ViewportGetter = std::function<std::pair<float, float>()>;

    /**
     * @brief Construct a MultiLaneVerticalAxisWidget
     * @param state State object providing lane descriptors (non-owning)
     * @param parent Parent widget
     */
    explicit MultiLaneVerticalAxisWidget(MultiLaneVerticalAxisState * state,
                                         QWidget * parent = nullptr);

    ~MultiLaneVerticalAxisWidget() override = default;

    /**
     * @brief Update the state pointer (e.g., when DataViewerState changes)
     *
     * Disconnects from the previous state and reconnects to the new one.
     *
     * @param state New state object (non-owning)
     */
    void setState(MultiLaneVerticalAxisState * state);

    /**
     * @brief Set the function to get the current effective Y viewport bounds
     * @param getter Function returning {y_min, y_max} in world coordinates
     */
    void setViewportGetter(ViewportGetter getter);

    /**
     * @brief Set the function to get the current vertical pan offset
     * @param getter Function returning y_pan in NDC units
     * @deprecated Use setViewportGetter instead
     */
    void setPanGetter(std::function<float()> getter);

    /**
     * @brief Set the visible Y range in NDC
     *
     * Determines which lanes are considered on-screen for culling.
     * Defaults to (-1, 1).
     *
     * @param y_min Minimum Y in NDC
     * @param y_max Maximum Y in NDC
     */
    void setYRange(float y_min, float y_max);

    /**
     * @brief Suggested size for the axis widget
     * @return QSize with fixed width of 50px
     */
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /**
     * @brief Emitted when the user completes a lane drag-and-drop reorder
     *
     * @param source_lane_id The stable lane_id of the dragged lane
     * @param target_visual_slot Target insertion position, 0-based from the top
     *        of the widget (0 = above topmost visible lane)
     */
    void laneReorderRequested(QString source_lane_id, int target_visual_slot);

    /**
     * @brief Emitted whenever the drag overlay visual state changes
     *
     * Fired on drag start, each mouse-move, and on drag end/cancel.
     * The OpenGL canvas connects to this to mirror the overlay across the
     * full plot area.
     *
     * @param active       True while a drag is in progress
     * @param lane_center  NDC Y center of the lane being dragged
     * @param lane_extent  NDC Y extent (height) of the dragged lane
     * @param marker_ndc_y NDC Y position of the current insertion-line marker
     */
    void laneDragOverlayChanged(bool active, float lane_center, float lane_extent, float marker_ndc_y);

protected:
    void paintEvent(QPaintEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void keyPressEvent(QKeyEvent * event) override;

private:
    MultiLaneVerticalAxisState * _state{nullptr};

    ViewportGetter _viewport_getter;
    float _y_min{-1.0f};
    float _y_max{1.0f};

    static constexpr int kAxisWidth = 50;

    // ---- Drag state ----
    bool _drag_active{false};
    int _dragged_lane_ndc_index{-1};///< Index into _state->lanes() of the dragged lane (NDC order)
    QString _drag_source_lane_id;   ///< Stable lane_id of the dragged lane
    int _current_insert_slot{0};    ///< Insertion slot, 0-based from top of widget
    QPoint _drag_current_pos;       ///< Last mouse position during drag

    /**
     * @brief Convert NDC Y value to pixel Y position
     *
     * Maps from normalized device coordinates (y_min at bottom, y_max at top)
     * to widget pixel coordinates (0 at top, height at bottom), incorporating
     * the current vertical pan offset.
     *
     * @param ndc_y Y value in NDC
     * @return Pixel Y position
     */
    [[nodiscard]] int ndcToPixelY(float ndc_y) const;

    /**
     * @brief Check if a lane is visible in the current viewport
     * @param center Lane center in NDC
     * @param extent Lane height in NDC
     * @return true if any part of the lane is within the viewport
     */
    [[nodiscard]] bool isLaneVisible(float center, float extent) const;

    /**
     * @brief Find the NDC-ordered lane index at a given pixel Y position
     * @param pixel_y Pixel Y coordinate in widget space
     * @return Index into _state->lanes() or -1 if no lane hit
     */
    [[nodiscard]] int laneIndexAtPixelY(int pixel_y) const;

    /**
     * @brief Compute the visual insertion slot for a pixel Y position
     *
     * Returns a slot in visual order (0 = above topmost lane).
     * Clamped to [0, N] where N is the number of lanes.
     *
     * @param pixel_y Pixel Y coordinate in widget space
     * @return Insertion slot index
     */
    [[nodiscard]] int insertSlotAtPixelY(int pixel_y) const;

    /**
     * @brief Compute the NDC Y coordinate of the insertion-line marker for a given slot
     *
     * Converts an integer visual insertion slot into the NDC y position of
     * the horizontal insertion line shown during drag feedback.
     *
     * @pre _state must be non-null and lanes must be non-empty
     * @param slot Visual insertion slot (0 = above topmost lane, N = below bottommost)
     * @return NDC Y value for the insertion marker
     */
    [[nodiscard]] float insertSlotNdcY(int slot) const;

    /**
     * @brief Reset all drag state
     */
    void resetDragState();
};

#endif// MULTI_LANE_VERTICAL_AXIS_WIDGET_HPP
