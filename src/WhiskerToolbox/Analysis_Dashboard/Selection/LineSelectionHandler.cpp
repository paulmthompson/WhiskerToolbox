#include "LineSelectionHandler.hpp"

#include "CoreGeometry/line_geometry.hpp"

#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>


LineSelectionHandler::LineSelectionHandler() {
    // Configure the controller with default styling
    CorePlotting::Interaction::LineInteractionConfig config;
    config.stroke_color = {1.0f, 0.0f, 0.0f, 1.0f}; // Bright red
    config.stroke_width = 5.0f; // Match previous line width
    _controller.setConfig(config);
    
    qDebug() << "LineSelectionHandler: Created (using CorePlotting controller)";
}

LineSelectionHandler::~LineSelectionHandler() = default;

void LineSelectionHandler::setNotificationCallback(NotificationCallback callback) {
    _notification_callback = callback;
}

void LineSelectionHandler::clearNotificationCallback() {
    _notification_callback = nullptr;
}

void LineSelectionHandler::startLineSelection(float world_x, float world_y, float screen_x, float screen_y) {

    qDebug() << "LineSelectionHandler: Starting line selection at world:" << world_x << "," << world_y
             << "screen:" << screen_x << "," << screen_y;

    // Store world coordinates for selection region creation
    _line_start_point_world = Point2D<float>(world_x, world_y);
    _line_end_point_world = _line_start_point_world; // Initially end point is same as start point
    
    // Start the controller in screen/canvas coordinates for rendering
    _controller.start(screen_x, screen_y, "line_selection");

    qDebug() << "LineSelectionHandler: Added first line point at world:" << world_x << "," << world_y;
}

void LineSelectionHandler::updateLineEndPoint(float world_x, float world_y, float screen_x, float screen_y) {
    if (!_controller.isActive()) {
        return;
    }

    // Update world coordinates for selection region
    _line_end_point_world = Point2D<float>(world_x, world_y);
    
    // Update controller with screen coordinates for rendering
    _controller.update(screen_x, screen_y);

    qDebug() << "LineSelectionHandler: Updated line end point to world:" << world_x << "," << world_y;
}

void LineSelectionHandler::completeLineSelection() {
    if (!_controller.isActive()) {
        qDebug() << "LineSelectionHandler: Cannot complete line selection - not currently drawing";
        cancelLineSelection();
        return;
    }

    qDebug() << "LineSelectionHandler: Completing line selection from"
             << _line_start_point_world.x << "," << _line_start_point_world.y
             << "to" << _line_end_point_world.x << "," << _line_end_point_world.y;

    // Complete the controller interaction
    _controller.complete();

    // Create selection region with world coordinates
    auto line_region = std::make_unique<LineSelectionRegion>(_line_start_point_world, _line_end_point_world);
    line_region->setBehavior(_current_behavior);
    // Set screen coordinates for picking
    line_region->setScreenCoordinates(_line_start_point_screen, _line_end_point_screen);
    _active_selection_region = std::move(line_region);

    // Call notification callback if set
    if (_notification_callback) {
        _notification_callback();
    }
}

void LineSelectionHandler::cancelLineSelection() {
    qDebug() << "LineSelectionHandler: Cancelling line selection";
    _controller.cancel();
}

CorePlotting::Interaction::GlyphPreview LineSelectionHandler::getPreview() const {
    return _controller.getPreview();
}

bool LineSelectionHandler::isActive() const {
    return _controller.isActive();
}

void LineSelectionHandler::mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) {
    qDebug() << "LineSelectionHandler::mousePressEvent called with button:" << event->button() 
             << "modifiers:" << event->modifiers() << "world_pos:" << world_pos.x() << world_pos.y();
             
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        qDebug() << "LineSelectionHandler: Ctrl+Left click detected!";
        if (!_controller.isActive()) {
            Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
            if (modifiers.testFlag(Qt::ShiftModifier)) {
                _current_behavior = LineSelectionBehavior::Remove;
            } else {
                _current_behavior = LineSelectionBehavior::Replace;
            }
            
            float screen_x = static_cast<float>(event->pos().x());
            float screen_y = static_cast<float>(event->pos().y());
            
            startLineSelection(world_pos.x(), world_pos.y(), screen_x, screen_y);
            // Store screen coordinates for picking
            _line_start_point_screen = Point2D<float>(screen_x, screen_y);
            _line_end_point_screen = _line_start_point_screen;
        }
    }
}

void LineSelectionHandler::mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (_controller.isActive() && (event->buttons() & Qt::LeftButton)) {
        float screen_x = static_cast<float>(event->pos().x());
        float screen_y = static_cast<float>(event->pos().y());
        
        qDebug() << "LineSelectionHandler: Updating line end point to" << world_pos.x() << "," << world_pos.y();
        updateLineEndPoint(world_pos.x(), world_pos.y(), screen_x, screen_y);
        // Update screen coordinates for picking
        _line_end_point_screen = Point2D<float>(screen_x, screen_y);
    }
}

void LineSelectionHandler::mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (_controller.isActive() && (event->button() == Qt::LeftButton)) {
        qDebug() << "LineSelectionHandler: Completing line selection";
        completeLineSelection();
    }
}

void LineSelectionHandler::keyPressEvent(QKeyEvent * event) {
    if (event->key() == Qt::Key_Escape) {
        if (_controller.isActive()) {
            cancelLineSelection();
        } else {
            if (_notification_callback) {
                _notification_callback();
            }
        }
    }
}

void LineSelectionHandler::deactivate() {
    cancelLineSelection();
}

// LineSelectionRegion implementation

LineSelectionRegion::LineSelectionRegion(Point2D<float> const & start_point, Point2D<float> const & end_point)
    : _start_point(start_point),
      _end_point(end_point) {
}

bool LineSelectionRegion::containsPoint(Point2D<float> point) const {
    // A point is "contained" if it's within a certain distance of the line
    auto const tolerance = 5.0; // 5 pixel tolerance
    auto distance2 = point_to_line_segment_distance2(point, _start_point, _end_point);
    return distance2 <= (tolerance * tolerance);
}

void LineSelectionRegion::getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const {
    min_x = std::min(_start_point.x, _end_point.x);
    min_y = std::min(_start_point.y, _end_point.y);
    max_x = std::max(_start_point.x, _end_point.x);
    max_y = std::max(_start_point.y, _end_point.y);
}