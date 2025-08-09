#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <QTest>

#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.hpp"
#include "Selection/SelectionModes.hpp"

#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame.hpp"
#include "CoreGeometry/points.hpp"

// Qt test fixtures (application setup)
#include "../fixtures/qt_test_fixtures.hpp"

namespace {

static QPoint worldToScreen(SpatialOverlayOpenGLWidget & widget, float world_x, float world_y) {
    // Derive projection bounds using screenToWorld at the corners
    QVector2D top_left = widget.screenToWorld(0, 0);
    QVector2D bottom_right = widget.screenToWorld(widget.width(), widget.height());

    float left = top_left.x();
    float top = top_left.y();
    float right = bottom_right.x();
    float bottom = bottom_right.y();

    if (left == right || top == bottom || widget.width() <= 0 || widget.height() <= 0) {
        return QPoint(0, 0);
    }

    int screen_x = static_cast<int>(((world_x - left) / (right - left)) * widget.width());
    int screen_y = static_cast<int>(((top - world_y) / (top - bottom)) * widget.height());
    return QPoint(screen_x, screen_y);
}

static bool waitForValidProjection(SpatialOverlayOpenGLWidget & widget, int timeout_ms = 500) {
    const int step = 10;
    int waited = 0;
    while (waited <= timeout_ms) {
        QVector2D tl = widget.screenToWorld(0, 0);
        QVector2D br = widget.screenToWorld(widget.width(), widget.height());
        if (tl.x() != br.x() && tl.y() != br.y()) {
            return true;
        }
        QTest::qWait(step);
        waited += step;
    }
    return false;
}

} // namespace

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - SpatialOverlayOpenGLWidget -SpatialOverlayOpenGLWidget emits bounds and supports point selection", "[SpatialOverlay][Selection]") {
 
    SpatialOverlayOpenGLWidget widget;
    widget.resize(400, 300);
    widget.show();
    processEvents();

    // Create simple point dataset with two points at known positions (ensure non-zero Y span)
    auto point_data = std::make_shared<PointData>();
    std::vector<Point2D<float>> frame_points = {Point2D<float>{100.0f, 100.0f}, Point2D<float>{200.0f, 150.0f}};
    point_data->overwritePointsAtTime(TimeFrameIndex(0), frame_points);

    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    widget.setPointData(map);
    processEvents();

    // Attach spy and trigger a deterministic view update emission
    QSignalSpy bounds_spy(&widget, &SpatialOverlayOpenGLWidget::viewBoundsChanged);

    // Ensure projection is valid first
    REQUIRE(waitForValidProjection(widget));

    // Change zoom slightly to force updateViewMatrices -> emits viewBoundsChanged
    widget.setZoomLevel(widget.getZoomLevel() + 0.01f);
    processEvents();

    if (bounds_spy.count() == 0) {
        // Fallback: tweak pan slightly to force another emission
        auto pan = widget.getPanOffset();
        widget.setPanOffset(pan.x() + 0.01f, pan.y());
        processEvents();
    }
    REQUIRE(bounds_spy.count() >= 1);

    // Enable point selection mode and zoom in to make world tolerance generous in screen space
    widget.setSelectionMode(SelectionMode::PointSelection);
    widget.setZoomLevel(5.0f);
    processEvents();
    // Note: selectionChanged carries size_t which QSignalSpy may not decode reliably.
    // Assert selection via state instead of spying the signal.

    // Ctrl+click near the first point (ensure focus and synthesize with modifiers)
    QPoint s0 = worldToScreen(widget, 100.0f, 100.0f);
    widget.raise();
    widget.activateWindow();
    widget.setFocus(Qt::OtherFocusReason);
    QTest::mouseMove(&widget, s0);
    QTest::qWait(10);

    QMouseEvent press(QEvent::MouseButtonPress, s0, widget.mapToGlobal(s0), Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    QCoreApplication::sendEvent(&widget, &press);
    processEvents();
    QMouseEvent release(QEvent::MouseButtonRelease, s0, widget.mapToGlobal(s0), Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
    QCoreApplication::sendEvent(&widget, &release);
    processEvents();
    REQUIRE(widget.getTotalSelectedPoints() >= 1);
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - SpatialOverlayOpenGLWidget - SpatialOverlayOpenGLWidget emits frameJumpRequested on double-click", "[SpatialOverlay][FrameJump]") {
    SpatialOverlayOpenGLWidget widget;
    widget.resize(400, 300);
    widget.show();
    processEvents();

    // Points in a known frame with non-zero Y span
    auto point_data = std::make_shared<PointData>();
    std::vector<Point2D<float>> frame_points = {Point2D<float>{150.0f, 150.0f}, Point2D<float>{180.0f, 200.0f}};
    point_data->overwritePointsAtTime(TimeFrameIndex(5), frame_points);

    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    widget.setPointData(map);
    processEvents();

    // Ensure projection is valid before interacting
    REQUIRE(waitForValidProjection(widget));

    QSignalSpy jump_spy(&widget, &SpatialOverlayOpenGLWidget::frameJumpRequested);

    QPoint s = worldToScreen(widget, 150.0f, 150.0f);
    QTest::mouseMove(&widget, s);
    QTest::qWait(10);
    QTest::mouseDClick(&widget, Qt::LeftButton, Qt::NoModifier, s);
    processEvents();

    if (jump_spy.count() == 0) {
        // Fallback: synthesize a double-click event directly
        QMouseEvent dbl(QEvent::MouseButtonDblClick, s, widget.mapToGlobal(s), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&widget, &dbl);
        processEvents();
    }

    REQUIRE(jump_spy.count() >= 1);
    QVariantList args = jump_spy.takeFirst();
    REQUIRE(args.size() == 2);

    auto frame_index = args.at(0).toLongLong();
    auto key = args.at(1).toString();

    REQUIRE(frame_index == 5);
    REQUIRE(key == QString("test_points"));
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - SpatialOverlayOpenGLWidget - polygon selection selects multiple points", "[SpatialOverlay][PolygonSelection]") {
    SpatialOverlayOpenGLWidget widget;
    widget.resize(400, 300);
    widget.show();
    processEvents();

    // Two points to be enclosed by a polygon
    auto point_data = std::make_shared<PointData>();
    std::vector<Point2D<float>> frame_points = {Point2D<float>{100.0f, 100.0f}, Point2D<float>{200.0f, 150.0f}};
    point_data->overwritePointsAtTime(TimeFrameIndex(0), frame_points);

    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    widget.setPointData(map);
    processEvents();

    REQUIRE(waitForValidProjection(widget));

    // Enter polygon selection mode
    widget.setSelectionMode(SelectionMode::PolygonSelection);
    processEvents();

    // Create a simple triangle that encloses both points
    QPoint a = worldToScreen(widget, 50.0f, 50.0f);
    QPoint b = worldToScreen(widget, 250.0f, 50.0f);
    QPoint c = worldToScreen(widget, 150.0f, 300.0f);

    widget.raise();
    widget.activateWindow();
    widget.setFocus(Qt::OtherFocusReason);
    QTest::mouseMove(&widget, a);
    QTest::qWait(5);

    // Left clicks to add vertices
    QMouseEvent p1(QEvent::MouseButtonPress, a, widget.mapToGlobal(a), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &p1);
    QMouseEvent r1(QEvent::MouseButtonRelease, a, widget.mapToGlobal(a), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &r1);

    QTest::mouseMove(&widget, b);
    QTest::qWait(5);
    QMouseEvent p2(QEvent::MouseButtonPress, b, widget.mapToGlobal(b), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &p2);
    QMouseEvent r2(QEvent::MouseButtonRelease, b, widget.mapToGlobal(b), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &r2);

    QTest::mouseMove(&widget, c);
    QTest::qWait(5);
    QMouseEvent p3(QEvent::MouseButtonPress, c, widget.mapToGlobal(c), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &p3);
    QMouseEvent r3(QEvent::MouseButtonRelease, c, widget.mapToGlobal(c), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &r3);

    // Press Enter to complete polygon and trigger selection
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &enterEvent);
    processEvents();

    // Expect both points selected
    REQUIRE(widget.getTotalSelectedPoints() >= 2);
}
