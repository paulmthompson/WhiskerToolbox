#ifndef LINESELECTIONHANDLER_HPP
#define LINESELECTIONHANDLER_HPP

#include "CoreGeometry/lines.hpp"
#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "CorePlotting/Interaction/LineInteractionController.hpp"
#include "ISelectionHandler.hpp"
#include "SelectionModes.hpp"

#include <QMatrix4x4>
#include <QPoint>
#include <QVector2D>

#include <functional>
#include <memory>
#include <vector>

class QKeyEvent;
class QMouseEvent;

enum class LineSelectionBehavior {
    Replace,
    Append,
    Remove
};

/**
 * @brief Line selection region for line-based selection
 */
class LineSelectionRegion : public SelectionRegion {
public:
    explicit LineSelectionRegion(Point2D<float> const & start_point, Point2D<float> const & end_point);

    [[nodiscard]] bool containsPoint(Point2D<float> point) const override;
    void getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const override;

    /**
     * @brief Get the start point of the line
     */
    [[nodiscard]] Point2D<float> const & getStartPoint() const { return _start_point; }

    /**
     * @brief Get the end point of the line
     */
    [[nodiscard]] Point2D<float> const & getEndPoint() const { return _end_point; }

    /**
     * @brief Get the start point of the line in screen coordinates
     */
    [[nodiscard]] Point2D<float> const & getStartPointScreen() const { return _start_point_screen; }

    /**
     * @brief Get the end point of the line in screen coordinates
     */
    [[nodiscard]] Point2D<float> const & getEndPointScreen() const { return _end_point_screen; }

    [[nodiscard]] LineSelectionBehavior getBehavior() const { return _behavior; }
    void setBehavior(LineSelectionBehavior behavior) { _behavior = behavior; }

    /**
     * @brief Set screen coordinates for picking
     * @param start_point_screen Start point in screen coordinates
     * @param end_point_screen End point in screen coordinates
     */
    void setScreenCoordinates(Point2D<float> const & start_point_screen, Point2D<float> const & end_point_screen) {
        _start_point_screen = start_point_screen;
        _end_point_screen = end_point_screen;
    }


private:
    Point2D<float> _start_point;
    Point2D<float> _end_point;
    Point2D<float> _start_point_screen;
    Point2D<float> _end_point_screen;
    LineSelectionBehavior _behavior = LineSelectionBehavior::Replace;
};

/**
 * @brief Handles line selection functionality for spatial overlay widgets
 * 
 * This class encapsulates all the logic needed for line selection,
 * including line drawing, rendering via PreviewRenderer, and selection region creation.
 * 
 * Internally delegates to CorePlotting::Interaction::LineInteractionController for
 * state management and preview generation. The widget's PreviewRenderer handles
 * actual OpenGL rendering.
 */
class LineSelectionHandler : public ISelectionHandler {
public:
    explicit LineSelectionHandler();
    ~LineSelectionHandler() override;

    // ISelectionHandler interface implementation
    void setNotificationCallback(NotificationCallback callback) override;
    void clearNotificationCallback() override;
    [[nodiscard]] CorePlotting::Interaction::GlyphPreview getPreview() const override;
    [[nodiscard]] bool isActive() const override;
    void deactivate() override;
    [[nodiscard]] std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const override { return _active_selection_region; }

    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) override;
    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) override;
    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) override;
    void keyPressEvent(QKeyEvent * event) override;

    /**
     * @brief Update line end point during drawing
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @param screen_x Screen X coordinate (for preview rendering)
     * @param screen_y Screen Y coordinate (for preview rendering)
     */
    void updateLineEndPoint(float world_x, float world_y, float screen_x, float screen_y);

private:
    NotificationCallback _notification_callback;

    // CorePlotting controller for state management and preview generation
    CorePlotting::Interaction::LineInteractionController _controller;

    // Line selection state (world coordinates for selection region)
    Point2D<float> _line_start_point_world;                   // Line start point in world coordinates
    Point2D<float> _line_end_point_world;                     // Line end point in world coordinates
    Point2D<float> _line_start_point_screen;                  // Line start point in screen coordinates (for picking)
    Point2D<float> _line_end_point_screen;                    // Line end point in screen coordinates (for picking)
    std::unique_ptr<SelectionRegion> _active_selection_region;// Current selection region
    LineSelectionBehavior _current_behavior = LineSelectionBehavior::Replace;

    /**
     * @brief Start line selection at given world coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @param screen_x Screen X coordinate (for preview rendering)
     * @param screen_y Screen Y coordinate (for preview rendering)
     */
    void startLineSelection(float world_x, float world_y, float screen_x, float screen_y);


    /**
     * @brief Complete line selection and create selection region
     */
    void completeLineSelection();

    /**
     * @brief Cancel current line selection
     */
    void cancelLineSelection();
};

#endif// LINESELECTIONHANDLER_HPP