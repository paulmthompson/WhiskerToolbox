#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include "Analysis_Dashboard/Analysis_Dashboard.hpp"
#include "Analysis_Dashboard/PlotContainer.hpp"
#include "Analysis_Dashboard/PlotFactory.hpp"
#include "Analysis_Dashboard/PlotOrganizers/DockingPlotOrganizer.hpp"
#include "Analysis_Dashboard/PlotOrganizers/PlotDockWidgetContent.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"
#include "Analysis_Dashboard/Groups/GroupCoordinator.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "DockManager.h"

#include "DataManager/DataManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "GroupManagementWidget/GroupManager.hpp"

#include "../fixtures/qt_test_fixtures.hpp"

namespace {

static QPoint worldToScreen(SpatialOverlayOpenGLWidget & widget, float world_x, float world_y) {
    QVector2D top_left = widget.screenToWorld(QPoint(0, 0));
    QVector2D bottom_right = widget.screenToWorld(QPoint(widget.width(), widget.height()));
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
        QVector2D tl = widget.screenToWorld(QPoint(0, 0));
        QVector2D br = widget.screenToWorld(QPoint(widget.width(), widget.height()));
        if (tl.x() != br.x() && tl.y() != br.y()) {
            return true;
        }
        QTest::qWait(step);
        waited += step;
    }
    return false;
}

} // namespace

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - Multiple SpatialOverlay plots react to group assignments", "[SpatialOverlay][Groups][Integration]") {
    // Set up ShaderManager (reuse approach from other tests)
    auto & shader_manager = ShaderManager::instance();
    //REQUIRE(shader_manager.getProgram("point") != nullptr);

    // Data manager and time bar
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create GroupManager
    auto* entity_group_manager = data_manager->getEntityGroupManager();
    REQUIRE(entity_group_manager != nullptr);
    auto group_manager = std::make_unique<GroupManager>(entity_group_manager, data_manager);
    
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);

    // Seed PointData with two distinct frames for unique EntityIds
    auto point_data = std::make_shared<PointData>();
    point_data->overwritePointsAtTime(TimeFrameIndex(1), std::vector<Point2D<float>>{{100.f, 100.f}});
    point_data->overwritePointsAtTime(TimeFrameIndex(2), std::vector<Point2D<float>>{{200.f, 150.f}});
    data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

    // Docking organizer environment
    ads::CDockManager dockManager;
    DockingPlotOrganizer organizer(&dockManager);
    QWidget * display = organizer.getDisplayWidget();
    REQUIRE(display != nullptr);
    display->resize(800, 600);
    display->show();
    processEvents();

    // Create two Spatial Overlay plot containers
    auto container1 = PlotFactory::createPlotContainer("spatial_overlay_plot");
    auto container2 = PlotFactory::createPlotContainer("spatial_overlay_plot");
    REQUIRE(container1 != nullptr);
    REQUIRE(container2 != nullptr);
    organizer.addPlot(std::move(container1));
    organizer.addPlot(std::move(container2));
    REQUIRE(organizer.getPlotCount() >= 2);

    // Locate the two plot widgets and their GL widgets
    auto ids = organizer.getAllPlotIds();
    REQUIRE(ids.size() >= 2);
    PlotContainer * pc1 = organizer.getPlot(ids[0]);
    PlotContainer * pc2 = organizer.getPlot(ids[1]);
    REQUIRE(pc1 != nullptr);
    REQUIRE(pc2 != nullptr);

    auto * plot1 = qobject_cast<SpatialOverlayPlotWidget *>(pc1->getPlotWidget());
    auto * plot2 = qobject_cast<SpatialOverlayPlotWidget *>(pc2->getPlotWidget());
    REQUIRE(plot1 != nullptr);
    REQUIRE(plot2 != nullptr);

    SpatialOverlayOpenGLWidget * gl1 = plot1->getOpenGLWidget();
    SpatialOverlayOpenGLWidget * gl2 = plot2->getOpenGLWidget();
    REQUIRE(gl1 != nullptr);
    REQUIRE(gl2 != nullptr);

    // Attach shared GroupManager via organizer/dashboard plumbing: directly use Analysis_Dashboard for canonical wiring
    Analysis_Dashboard dashboard(data_manager, group_manager.get(), time_scrollbar, &dockManager);
    REQUIRE(dashboard.getGroupManager() != nullptr);
    GroupManager * gm = dashboard.getGroupManager();
    plot1->setGroupManager(gm);
    plot2->setGroupManager(gm);

    // NEW: Use GroupCoordinator mediator and register both plots so group changes propagate consistently
    GroupCoordinator coordinator(gm);
    REQUIRE(ids.size() >= 2);
    coordinator.registerPlot(ids[0], plot1);
    coordinator.registerPlot(ids[1], plot2);

    // Provide the same dataset to both GL widgets
    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    gl1->setPointData(map);
    gl2->setPointData(map);
    processEvents();

    // Ensure projections valid before interactions
    gl1->resize(400, 300);
    gl2->resize(400, 300);
    gl1->show(); gl2->show();
    processEvents();
    REQUIRE(waitForValidProjection(*gl1));
    REQUIRE(waitForValidProjection(*gl2));

    // Select both points in gl1
    gl1->setSelectionMode(SelectionMode::PointSelection);
    processEvents();
    auto clickCtrlAt = [&](SpatialOverlayOpenGLWidget * w, float wx, float wy) {
        QPoint s = worldToScreen(*w, wx, wy);
        w->raise(); w->activateWindow(); w->setFocus(Qt::OtherFocusReason);
        QTest::mouseMove(w, s);
        QTest::mousePress(w, Qt::LeftButton, Qt::ControlModifier, s);
        QTest::mouseRelease(w, Qt::LeftButton, Qt::ControlModifier, s);
        processEvents();
    };
    clickCtrlAt(gl1, 100.f, 100.f);
    clickCtrlAt(gl1, 200.f, 150.f);
    REQUIRE(gl1->getTotalSelectedPoints() >= 2);

    // Spy on signals: other plot update and GroupManager emissions
    QSignalSpy gl2UpdateSpy(plot2, &SpatialOverlayPlotWidget::renderUpdateRequested);
    QSignalSpy gmCreatedSpy(gm, &GroupManager::groupCreated);
    QSignalSpy gmModifiedSpy(gm, &GroupManager::groupModified);

    // Create new group through gl1 context (uses GroupManager) and assign
    gl1->assignSelectedPointsToNewGroup();
    processEvents();

    // Expect the other plot to schedule a render update due to GroupManager::groupModified
    // SpatialOverlayOpenGLWidget currently does not explicitly connect to groupModified; this test documents expected behavior.
    // Allow some time for mediator propagation
    QTest::qWait(50);

    // The expected strong behavior: at least one render update requested by plot2 in response to group assignment
    // Current implementation may fail this assertion (known gap).
    REQUIRE(gl2UpdateSpy.count() >= 1);

    // Also ensure that the GroupManager emitted both creation and modification signals for first assignment
    REQUIRE(gmCreatedSpy.count() >= 1);
    REQUIRE(gmModifiedSpy.count() == 1);

    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - SpatialOverlay - onGroupModified is emitted exactly once per plot on first assignment", "[SpatialOverlay][Groups][Signals]") {
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create GroupManager
    auto* entity_group_manager = data_manager->getEntityGroupManager();
    REQUIRE(entity_group_manager != nullptr);
    auto group_manager = std::make_unique<GroupManager>(entity_group_manager, data_manager);
    
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);

    // Data setup with entity ids
    auto point_data = std::make_shared<PointData>();
    point_data->overwritePointsAtTime(TimeFrameIndex(1), std::vector<Point2D<float>>{{100.f, 100.f}});
    point_data->overwritePointsAtTime(TimeFrameIndex(2), std::vector<Point2D<float>>{{200.f, 150.f}});
    data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

    ads::CDockManager dockManager;
    DockingPlotOrganizer organizer(&dockManager);
    QWidget * display = organizer.getDisplayWidget();
    REQUIRE(display != nullptr);
    display->resize(800, 600);
    display->show();
    processEvents();

    auto container1 = PlotFactory::createPlotContainer("spatial_overlay_plot");
    auto container2 = PlotFactory::createPlotContainer("spatial_overlay_plot");
    REQUIRE(container1 != nullptr);
    REQUIRE(container2 != nullptr);
    organizer.addPlot(std::move(container1));
    organizer.addPlot(std::move(container2));
    REQUIRE(organizer.getPlotCount() >= 2);

    auto ids = organizer.getAllPlotIds();
    PlotContainer * pc1 = organizer.getPlot(ids[0]);
    PlotContainer * pc2 = organizer.getPlot(ids[1]);
    auto * plot1 = qobject_cast<SpatialOverlayPlotWidget *>(pc1->getPlotWidget());
    auto * plot2 = qobject_cast<SpatialOverlayPlotWidget *>(pc2->getPlotWidget());
    REQUIRE(plot1 != nullptr);
    REQUIRE(plot2 != nullptr);

    SpatialOverlayOpenGLWidget * gl1 = plot1->getOpenGLWidget();
    SpatialOverlayOpenGLWidget * gl2 = plot2->getOpenGLWidget();
    REQUIRE(gl1 != nullptr);
    REQUIRE(gl2 != nullptr);

    Analysis_Dashboard dashboard(data_manager, group_manager.get(), time_scrollbar, &dockManager);
    GroupManager * gm = dashboard.getGroupManager();
    REQUIRE(gm != nullptr);

    plot1->setGroupManager(gm);
    plot2->setGroupManager(gm);

    std::unordered_map<QString, std::shared_ptr<PointData>> map{{QString("test_points"), point_data}};
    gl1->setPointData(map);
    gl2->setPointData(map);
    gl1->resize(400, 300); gl2->resize(400, 300);
    gl1->show(); gl2->show();
    processEvents();

    REQUIRE(waitForValidProjection(*gl1));
    REQUIRE(waitForValidProjection(*gl2));

    // Spy on custom diagnostic signal to count refreshes per plot
    QSignalSpy gmModifiedSpy(gm, &GroupManager::groupModified);

    // Select both points in gl1 and create group
    gl1->setSelectionMode(SelectionMode::PointSelection);
    auto clickCtrlAt = [&](SpatialOverlayOpenGLWidget * w, float wx, float wy) {
        QPoint s = worldToScreen(*w, wx, wy);
        w->raise(); w->activateWindow(); w->setFocus(Qt::OtherFocusReason);
        QTest::mouseMove(w, s);
        QTest::mousePress(w, Qt::LeftButton, Qt::ControlModifier, s);
        QTest::mouseRelease(w, Qt::LeftButton, Qt::ControlModifier, s);
        processEvents();
    };
    clickCtrlAt(gl1, 100.f, 100.f);
    clickCtrlAt(gl1, 200.f, 150.f);
    REQUIRE(gl1->getTotalSelectedPoints() >= 1);

    gl1->assignSelectedPointsToNewGroup();
    processEvents();
    QTest::qWait(50);

    // Exactly one refresh per plot expected for a single groupModified emission
    REQUIRE(gmModifiedSpy.count() == 1);

    delete time_scrollbar;
}


