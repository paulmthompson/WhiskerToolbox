#ifndef POLYGONSELECTIONHANDLER_HPP
#define POLYGONSELECTIONHANDLER_HPP

#include "CoreGeometry/points.hpp"
#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "CorePlotting/Interaction/PolygonInteractionController.hpp"
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

/**
 * @brief Polygon selection region for area-based selection
 */
class PolygonSelectionRegion : public SelectionRegion {
public:
    explicit PolygonSelectionRegion(std::vector<Point2D<float>> const & vertices);

    bool containsPoint(Point2D<float> point) const override;
    void getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const override;

    /**
     * @brief Get the polygon vertices in world coordinates
     */
    [[nodiscard]] std::vector<Point2D<float>> const & getVertices() const { return _polygon.getVertices(); }

private:
    Polygon _polygon;
};

/**
 * @brief Handles polygon selection functionality for spatial overlay widgets
 * 
 * This class encapsulates all the logic needed for polygon selection,
 * including vertex management, rendering via PreviewRenderer, and selection region creation.
 * 
 * Internally delegates to CorePlotting::Interaction::PolygonInteractionController for
 * state management and preview generation. The widget's PreviewRenderer handles
 * actual OpenGL rendering.
 */
class PolygonSelectionHandler : public ISelectionHandler {
public:
    explicit PolygonSelectionHandler();
    ~PolygonSelectionHandler() override;

    // ISelectionHandler interface implementation
    void setNotificationCallback(NotificationCallback callback) override;
    void clearNotificationCallback() override;
    [[nodiscard]] CorePlotting::Interaction::GlyphPreview getPreview() const override;
    [[nodiscard]] bool isActive() const override;
    void deactivate() override;
    [[nodiscard]] std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const override { return _active_selection_region; }

    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) override;
    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) override;
    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) override {}
    void keyPressEvent(QKeyEvent * event) override;

private:
    NotificationCallback _notification_callback;

    // CorePlotting controller for state management and preview generation
    CorePlotting::Interaction::PolygonInteractionController _controller;

    // Polygon selection state (world coordinates for selection region)
    std::vector<Point2D<float>> _polygon_vertices_world;      // Polygon vertices in world coordinates
    std::unique_ptr<SelectionRegion> _active_selection_region;// Current selection region

    /**
     * @brief Check if currently in polygon selection mode
     * @return True if actively selecting a polygon
     */
    [[nodiscard]] bool isPolygonSelecting() const { return _controller.isActive(); }

    /**
     * @brief Get the number of vertices in the current polygon
     * @return Number of vertices
     */
    [[nodiscard]] size_t getVertexCount() const { return _controller.getVertexCount(); }

    /**
     * @brief Start polygon selection at given world coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @param screen_x Screen X coordinate (for preview rendering)
     * @param screen_y Screen Y coordinate (for preview rendering)
     */
    void startPolygonSelection(float world_x, float world_y, float screen_x, float screen_y);

    /**
     * @brief Add point to current polygon selection
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @param screen_x Screen X coordinate (for preview rendering)
     * @param screen_y Screen Y coordinate (for preview rendering)
     */
    void addPolygonVertex(float world_x, float world_y, float screen_x, float screen_y);

    /**
     * @brief Complete polygon selection and create selection region
     */
    void completePolygonSelection();

    /**
     * @brief Cancel current polygon selection
     */
    void cancelPolygonSelection();
};

#endif// POLYGONSELECTIONHANDLER_HPP
