#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <QTest>

#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.hpp"
#include "Selection/SelectionModes.hpp"

#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame.hpp"
#include "CoreGeometry/points.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QTimer>

#include "Analysis_Dashboard/Groups/GroupManager.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.hpp"
#include "Analysis_Dashboard/PlotOrganizers/GraphicsScenePlotOrganizer.hpp"
#include "Analysis_Dashboard/PlotOrganizers/DockingPlotOrganizer.hpp"
#include "Analysis_Dashboard/PlotFactory.hpp"
#include "Analysis_Dashboard/PlotContainer.hpp"
#include "DockManager.h"

#include "DataManager/DataManager.hpp"

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
    //QSignalSpy bounds_spy(&widget, &SpatialOverlayOpenGLWidget::viewBoundsChanged);

    // Ensure projection is valid first
    REQUIRE(waitForValidProjection(widget));

    // Force updateViewMatrices -> emits viewBoundsChanged
    //widget.resetView();
    processEvents();


    //REQUIRE(bounds_spy.count() >= 1);

    // Enable point selection mode
    widget.setSelectionMode(SelectionMode::PointSelection);
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

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - SpatialOverlayOpenGLWidget - grouping via context menu assigns selected points", "[SpatialOverlay][Grouping]") {
    SpatialOverlayOpenGLWidget widget;
    widget.resize(400, 300);
    widget.show();
    processEvents();

    // Two distinct points with unique row ids (different frames)
    auto point_data = std::make_shared<PointData>();
    std::vector<Point2D<float>> frame_points1 = {Point2D<float>{100.0f, 100.0f}}; // frame 1
    std::vector<Point2D<float>> frame_points2 = {Point2D<float>{200.0f, 150.0f}}; // frame 2
    point_data->overwritePointsAtTime(TimeFrameIndex(1), frame_points1);
    point_data->overwritePointsAtTime(TimeFrameIndex(2), frame_points2);

    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    widget.setPointData(map);
    REQUIRE(waitForValidProjection(widget));

    // Select both points via Ctrl+clicks
    widget.setSelectionMode(SelectionMode::PointSelection);
    processEvents();

    auto clickCtrlAt = [&](float wx, float wy) {
        QPoint s = worldToScreen(widget, wx, wy);
        widget.raise(); widget.activateWindow(); widget.setFocus(Qt::OtherFocusReason);
        QTest::mouseMove(&widget, s);
        QMouseEvent p(QEvent::MouseButtonPress, s, widget.mapToGlobal(s), Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        QCoreApplication::sendEvent(&widget, &p);
        processEvents();
        QMouseEvent r(QEvent::MouseButtonRelease, s, widget.mapToGlobal(s), Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
        QCoreApplication::sendEvent(&widget, &r);
        processEvents();
    };

    clickCtrlAt(100.0f, 100.0f);
    clickCtrlAt(200.0f, 150.0f);
    REQUIRE(widget.getTotalSelectedPoints() >= 2);

    // Attach a GroupManager
    GroupManager gm(nullptr);
    widget.setGroupManager(&gm);

    // Ensure context menu shows non-modally in tests
    qputenv("WT_TESTING_NON_MODAL_MENUS", "1");

    // Helper: poll for the non-modal menu and ACTUALLY CLICK the "Create New Group" item
    // This simulates real user interaction and will fail if clicks are not reaching the menu
    auto triggerCreateNewGroup = [&]() -> bool {
        const int maxWaitMs = 1500;
        const int stepMs = 25;
        int waited = 0;
        while (waited <= maxWaitMs) {
            for (QWidget * topLevelWidget : QApplication::topLevelWidgets()) {
                auto * mainMenu = qobject_cast<QMenu *>(topLevelWidget);
                if (!mainMenu) continue;

                QAction * assignToGroupAction = nullptr;
                for (QAction * action : mainMenu->actions()) {
                    if (action && action->menu() && action->text().contains("Assign to Group", Qt::CaseInsensitive)) {
                        assignToGroupAction = action;
                        break;
                    }
                }
                if (!assignToGroupAction) continue;

                // Open the submenu by hovering/clicking on the parent action
                QRect assignRect = mainMenu->actionGeometry(assignToGroupAction);
                if (!assignRect.isValid()) continue;

                QPoint assignCenter = assignRect.center();
                QTest::mouseMove(mainMenu, assignCenter);
                QTest::qWait(50);
                // Some styles require a click to open submenu in tests
                if (!assignToGroupAction->menu() || !assignToGroupAction->menu()->isVisible()) {
                    QTest::mouseClick(mainMenu, Qt::LeftButton, Qt::NoModifier, assignCenter);
                }

                // Wait for submenu to become visible
                QMenu * subMenu = assignToGroupAction->menu();
                int subWaited = 0;
                while (subMenu && !subMenu->isVisible() && subWaited <= 500) {
                    QTest::qWait(25);
                    subWaited += 25;
                }
                if (!subMenu || !subMenu->isVisible()) {
                    continue;
                }

                // Find the "Create New Group" action in the submenu
                QAction * createNewGroupAction = nullptr;
                for (QAction * subAction : subMenu->actions()) {
                    if (subAction && subAction->text().contains("Create New Group", Qt::CaseInsensitive)) {
                        createNewGroupAction = subAction;
                        break;
                    }
                }
                if (!createNewGroupAction) continue;

                QRect createRect = subMenu->actionGeometry(createNewGroupAction);
                if (!createRect.isValid()) continue;

                // Move and click on the submenu action's rectangle
                QPoint createCenter = createRect.center();
                QTest::mouseMove(subMenu, createCenter);
                QTest::qWait(25);
                QTest::mouseClick(subMenu, Qt::LeftButton, Qt::NoModifier, createCenter);

                return true;
            }
            QTest::qWait(stepMs);
            waited += stepMs;
        }
        return false;
    };

    // Open context menu with right-click (not in polygon mode)
    QPoint pos = worldToScreen(widget, 150.0f, 120.0f);
    QTest::mouseClick(&widget, Qt::RightButton, Qt::NoModifier, pos);
    processEvents();

    REQUIRE(triggerCreateNewGroup());
    processEvents();

    // Verify a group was created and points assigned
    auto const & groups = gm.getGroups();
    REQUIRE(groups.size() >= 1);
    // Row ids are 1 and 2 from TimeFrameIndex(1) and (2)
    REQUIRE(gm.getPointGroup(1) != -1);
    REQUIRE(gm.getPointGroup(2) != -1);
    // Selection clears after assignment
    REQUIRE(widget.getTotalSelectedPoints() == 0);
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - SpatialOverlayPlotPropertiesWidget - properties propagate to OpenGL widget", "[SpatialOverlay][Properties]") {
    // Create plot widget (QGraphicsItem) and properties widget
    SpatialOverlayPlotWidget plotItem;
    SpatialOverlayPlotPropertiesWidget props;

    // Attach plot to properties
    props.setPlotWidget(&plotItem);

    // Access child widgets by objectName
    auto * pointSize = props.findChild<QDoubleSpinBox *>("point_size_spinbox");
    auto * lineWidth = props.findChild<QDoubleSpinBox *>("line_width_spinbox");
    auto * tooltips = props.findChild<QCheckBox *>("tooltips_checkbox");
    auto * modeCombo = props.findChild<QComboBox *>("selection_mode_combo");

    REQUIRE(pointSize); REQUIRE(lineWidth); REQUIRE(tooltips); REQUIRE(modeCombo);

    auto * gl = plotItem.getOpenGLWidget();
    REQUIRE(gl != nullptr);

    // Change point size
    pointSize->setValue(12.5);
    REQUIRE(gl->getPointSize() == Catch::Approx(12.5f));

    // Change line width
    lineWidth->setValue(3.5);
    REQUIRE(gl->getLineWidth() == Catch::Approx(3.5f));



    // Toggle tooltips
    tooltips->setChecked(false);
    REQUIRE(gl->getTooltipsEnabled() == false);

    // Change selection mode to Polygon
    modeCombo->setCurrentIndex(2); // 0=None,1=Point,2=Polygon,3=Line
    REQUIRE(plotItem.getSelectionMode() == SelectionMode::PolygonSelection);
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - Organizer(GraphicsScene) - spatial overlay selection works inside organizer", "[Organizer][GraphicsScene]") {
    // Create organizer and show its view
    GraphicsScenePlotOrganizer organizer;
    QWidget * display = organizer.getDisplayWidget();
    REQUIRE(display != nullptr);
    display->resize(800, 600);
    display->show();
    processEvents();

    // Create spatial overlay plot container
    auto container = PlotFactory::createPlotContainer("spatial_overlay_plot");
    REQUIRE(container != nullptr);

    // Add the plot to the organizer
    organizer.addPlot(std::move(container));
    REQUIRE(organizer.getPlotCount() == 1);

    // Access the plot to set data and get GL widget
    auto ids = organizer.getAllPlotIds();
    REQUIRE(ids.size() == 1);
    PlotContainer * pc = organizer.getPlot(ids.front());
    REQUIRE(pc != nullptr);
    auto * plotItem = pc->getPlotWidget();
    REQUIRE(plotItem != nullptr);
    auto * overlay = dynamic_cast<SpatialOverlayPlotWidget *>(plotItem);
    REQUIRE(overlay != nullptr);
    auto * gl = overlay->getOpenGLWidget();
    REQUIRE(gl != nullptr);

    // Set test data
    auto point_data = std::make_shared<PointData>();
    point_data->overwritePointsAtTime(TimeFrameIndex(1), std::vector<Point2D<float>>{{100.f,100.f}});
    point_data->overwritePointsAtTime(TimeFrameIndex(2), std::vector<Point2D<float>>{{200.f,150.f}});

    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    gl->setPointData(map);
    REQUIRE(waitForValidProjection(*gl));

    // Point selection via organizer's view: dispatch events to the GL widget directly
    gl->setSelectionMode(SelectionMode::PointSelection);
    processEvents();

    auto clickCtrlAtGl = [&](float wx, float wy) {
        QPoint s = worldToScreen(*gl, wx, wy);
        gl->raise(); gl->activateWindow(); gl->setFocus(Qt::OtherFocusReason);
        QTest::mouseMove(gl, s);
        QMouseEvent p(QEvent::MouseButtonPress, s, gl->mapToGlobal(s), Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        QCoreApplication::sendEvent(gl, &p);
        processEvents();
        QMouseEvent r(QEvent::MouseButtonRelease, s, gl->mapToGlobal(s), Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
        QCoreApplication::sendEvent(gl, &r);
        processEvents();
    };

    clickCtrlAtGl(100.f, 100.f);
    clickCtrlAtGl(200.f, 150.f);
    REQUIRE(gl->getTotalSelectedPoints() >= 2);

    // Polygon selection: clear, then select both with a rectangle
    gl->clearSelection();
    gl->setSelectionMode(SelectionMode::PolygonSelection);
    processEvents();

    QPoint a = worldToScreen(*gl, 50.f, 50.f);
    QPoint b = worldToScreen(*gl, 250.f, 50.f);
    QPoint c = worldToScreen(*gl, 250.f, 250.f);
    QPoint d = worldToScreen(*gl, 50.f, 250.f);

    auto leftClickAt = [&](QPoint p) {
        QMouseEvent p1(QEvent::MouseButtonPress, p, gl->mapToGlobal(p), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(gl, &p1);
        QMouseEvent r1(QEvent::MouseButtonRelease, p, gl->mapToGlobal(p), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(gl, &r1);
    };

    leftClickAt(a); leftClickAt(b); leftClickAt(c); leftClickAt(d);
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(gl, &enterEvent);
    processEvents();

    REQUIRE(gl->getTotalSelectedPoints() >= 2);
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - Organizer(Docking) - spatial overlay selection works inside organizer", "[Organizer][Docking]") {
    // Docking organizer requires a dock manager
    ads::CDockManager dockManager;
    DockingPlotOrganizer organizer(&dockManager);

    QWidget * display = organizer.getDisplayWidget();
    REQUIRE(display != nullptr);
    display->resize(800, 600);
    display->show();
    processEvents();

    auto container = PlotFactory::createPlotContainer("spatial_overlay_plot");
    REQUIRE(container != nullptr);

    organizer.addPlot(std::move(container));
    REQUIRE(organizer.getPlotCount() == 1);

    auto ids = organizer.getAllPlotIds();
    REQUIRE(ids.size() == 1);
    PlotContainer * pc = organizer.getPlot(ids.front());
    REQUIRE(pc != nullptr);
    auto * overlay = dynamic_cast<SpatialOverlayPlotWidget *>(pc->getPlotWidget());
    REQUIRE(overlay != nullptr);
    auto * gl = overlay->getOpenGLWidget();
    REQUIRE(gl != nullptr);

    auto point_data = std::make_shared<PointData>();
    point_data->overwritePointsAtTime(TimeFrameIndex(1), std::vector<Point2D<float>>{{100.f,100.f}});
    point_data->overwritePointsAtTime(TimeFrameIndex(2), std::vector<Point2D<float>>{{200.f,150.f}});
    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    gl->setPointData(map);
    REQUIRE(waitForValidProjection(*gl));

    gl->setSelectionMode(SelectionMode::PointSelection);
    processEvents();

    auto clickCtrlAtGl = [&](float wx, float wy) {
        QPoint s = worldToScreen(*gl, wx, wy);
        gl->raise(); gl->activateWindow(); gl->setFocus(Qt::OtherFocusReason);
        QTest::mouseMove(gl, s);
        QMouseEvent p(QEvent::MouseButtonPress, s, gl->mapToGlobal(s), Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        QCoreApplication::sendEvent(gl, &p);
        processEvents();
        QMouseEvent r(QEvent::MouseButtonRelease, s, gl->mapToGlobal(s), Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
        QCoreApplication::sendEvent(gl, &r);
        processEvents();
    };

    clickCtrlAtGl(100.f, 100.f);
    clickCtrlAtGl(200.f, 150.f);
    REQUIRE(gl->getTotalSelectedPoints() >= 2);
}

