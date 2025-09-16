#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>

#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"

#include "CoreGeometry/points.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "../fixtures/qt_test_fixtures.hpp"

TEST_CASE_METHOD(QtWidgetTestFixture, "Analysis Dashboard - SpatialOverlayPlotWidget - setPointDataKeys triggers a single visualization update", "[SpatialOverlay][Widget][Update]") {
    // Create the plot widget (QGraphicsItem)
    SpatialOverlayPlotWidget plotItem;

    // Prepare a DataManager with a small PointData dataset
    auto dm = std::make_shared<DataManager>();

    auto point_data = std::make_shared<PointData>();
    std::vector<Point2D<float>> frame_points = {Point2D<float>{100.0f, 100.0f}, Point2D<float>{200.0f, 150.0f}};
    point_data->overwritePointsAtTime(TimeFrameIndex(0), frame_points);

    dm->setData<PointData>("test_points", point_data, TimeKey("time"));

    // Attach DataManager to the plot widget
    plotItem.setDataManager(dm);

    // Spies to count updates
    QSignalSpy spyRender(&plotItem, &AbstractPlotWidget::renderUpdateRequested);
    REQUIRE(spyRender.isValid());

    SpatialOverlayOpenGLWidget * gl = plotItem.getOpenGLWidget();
    REQUIRE(gl != nullptr);


    // Trigger visualization by enabling the point data key
    plotItem.setDataKeys(QStringList{QString::fromStdString("test_points")},
                         QStringList{},
                         QStringList{});
    processEvents();

    // Expect exactly one visualization update from this action
    REQUIRE(spyRender.count() == 1);
}
