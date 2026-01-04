#include "PolygonSelectionHandler.hpp"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>


PolygonSelectionHandler::PolygonSelectionHandler() {
    // Configure the controller with default styling
    CorePlotting::Interaction::PolygonInteractionConfig config;
    config.stroke_color = {0.2f, 0.6f, 1.0f, 1.0f};  // Blue
    config.vertex_color = {1.0f, 0.0f, 0.0f, 1.0f};  // Red vertices
    config.closure_line_color = {1.0f, 0.6f, 0.2f, 1.0f}; // Orange closure
    config.stroke_width = 2.0f;
    config.vertex_size = 8.0f;
    _controller.setConfig(config);
    
    qDebug() << "PolygonSelectionHandler: Created (using CorePlotting controller)";
}

PolygonSelectionHandler::~PolygonSelectionHandler() = default;

void PolygonSelectionHandler::setNotificationCallback(NotificationCallback callback) {
    _notification_callback = callback;
}

void PolygonSelectionHandler::clearNotificationCallback() {
    _notification_callback = nullptr;
}

void PolygonSelectionHandler::startPolygonSelection(float world_x, float world_y, float screen_x, float screen_y) {

    qDebug() << "PolygonSelectionHandler: Starting polygon selection at world:" << world_x << "," << world_y
             << "screen:" << screen_x << "," << screen_y;

    // Clear any previous world coordinates
    _polygon_vertices_world.clear();
    
    // Add first vertex to world coordinates storage
    _polygon_vertices_world.push_back(Point2D<float>(world_x, world_y));

    // Start the controller in screen/canvas coordinates for rendering
    _controller.start(screen_x, screen_y, "polygon_selection");

    qDebug() << "PolygonSelectionHandler: Added first polygon vertex at world:" << world_x << "," << world_y;
}

void PolygonSelectionHandler::addPolygonVertex(float world_x, float world_y, float screen_x, float screen_y) {
    if (!_controller.isActive()) {
        return;
    }

    // Add vertex to world coordinates storage
    _polygon_vertices_world.push_back(Point2D<float>(world_x, world_y));
    
    // Add vertex to controller for rendering
    auto result = _controller.addVertex(screen_x, screen_y);

    qDebug() << "PolygonSelectionHandler: Added polygon vertex" << _polygon_vertices_world.size()
             << "at world:" << world_x << "," << world_y;
             
    // Check if polygon was auto-closed by clicking near first vertex
    if (result == CorePlotting::Interaction::AddVertexResult::ClosedPolygon) {
        qDebug() << "PolygonSelectionHandler: Polygon auto-closed by clicking near first vertex";
        completePolygonSelection();
    }
}

void PolygonSelectionHandler::completePolygonSelection() {
    if (!_controller.isActive() || _polygon_vertices_world.size() < 3) {
        qDebug() << "PolygonSelectionHandler: Cannot complete polygon selection - insufficient vertices";
        cancelPolygonSelection();
        return;
    }

    qDebug() << "PolygonSelectionHandler: Completing polygon selection with"
             << _polygon_vertices_world.size() << "vertices";

    // Complete the controller interaction
    _controller.complete();

    // Create selection region with world coordinates
    auto polygon_region = std::make_unique<PolygonSelectionRegion>(_polygon_vertices_world);
    _active_selection_region = std::move(polygon_region);

    // Call notification callback if set
    if (_notification_callback) {
        _notification_callback();
    }

    // Clean up polygon selection state
    _polygon_vertices_world.clear();
}

void PolygonSelectionHandler::cancelPolygonSelection() {
    qDebug() << "PolygonSelectionHandler: Cancelling polygon selection";

    _controller.cancel();
    _polygon_vertices_world.clear();
}

CorePlotting::Interaction::GlyphPreview PolygonSelectionHandler::getPreview() const {
    return _controller.getPreview();
}

bool PolygonSelectionHandler::isActive() const {
    return _controller.isActive();
}

void PolygonSelectionHandler::mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        float screen_x = static_cast<float>(event->pos().x());
        float screen_y = static_cast<float>(event->pos().y());
        
        if (!isPolygonSelecting()) {
            startPolygonSelection(world_pos.x(), world_pos.y(), screen_x, screen_y);
        } else {
            addPolygonVertex(world_pos.x(), world_pos.y(), screen_x, screen_y);
        }
    }
}

void PolygonSelectionHandler::mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) {
    // Update cursor position for preview line from last vertex to cursor
    if (_controller.isActive()) {
        float screen_x = static_cast<float>(event->pos().x());
        float screen_y = static_cast<float>(event->pos().y());
        _controller.updateCursorPosition(screen_x, screen_y);
    }
}

void PolygonSelectionHandler::keyPressEvent(QKeyEvent * event) {
    if (event->key() == Qt::Key_Escape) {
        cancelPolygonSelection();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (isPolygonSelecting()) {
            completePolygonSelection();
        }
    }
}

void PolygonSelectionHandler::deactivate() {
    cancelPolygonSelection();
}

PolygonSelectionRegion::PolygonSelectionRegion(std::vector<Point2D<float>> const & vertices)
    : _polygon(vertices) {
}

bool PolygonSelectionRegion::containsPoint(Point2D<float> point) const {
    return _polygon.containsPoint(point);
}

void PolygonSelectionRegion::getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const {
    auto bbox = _polygon.getBoundingBox();
    min_x = bbox.min_x;
    min_y = bbox.min_y;
    max_x = bbox.max_x;
    max_y = bbox.max_y;
}
