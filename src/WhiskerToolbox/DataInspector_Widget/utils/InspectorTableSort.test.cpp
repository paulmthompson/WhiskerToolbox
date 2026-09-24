/**
 * @file InspectorTableSort.test.cpp
 * @brief Unit tests for Data Inspector table model sorting
 */

#include "DataInspector_Widget/DigitalEventSeries/EventTableModel.hpp"
#include "DataInspector_Widget/DigitalIntervalSeries/IntervalTableModel.hpp"
#include "DataInspector_Widget/LineData/LineTableModel.hpp"
#include "DataInspector_Widget/MaskData/MaskTableModel.hpp"
#include "DataInspector_Widget/PointData/PointTableModel.hpp"

#include "CoreGeometry/points.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

namespace {

void ensureQCoreApplication() {
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QCoreApplication(argc, argv.data());// NOLINT: Intentionally leaked
    }
}

[[nodiscard]] Mask2D makeMask(std::size_t point_count) {
    Mask2D mask;
    for (std::size_t i = 0; i < point_count; ++i) {
        mask.push_back(Point2D<uint32_t>{static_cast<uint32_t>(i), static_cast<uint32_t>(i)});
    }
    return mask;
}

[[nodiscard]] Line2D makeLine(std::size_t point_count) {
    Line2D line;
    for (std::size_t i = 0; i < point_count; ++i) {
        line.push_back(Point2D<float>{static_cast<float>(i), static_cast<float>(i)});
    }
    return line;
}

[[nodiscard]] std::shared_ptr<TimeFrame> makeTimeFrame(int count) {
    std::vector<int> times(count);
    std::iota(times.begin(), times.end(), 0);
    return std::make_shared<TimeFrame>(times);
}

}// namespace

TEST_CASE("MaskTableModel sorts display rows", "[InspectorTableSort][MaskTableModel]") {
    ensureQCoreApplication();

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(makeTimeFrame(100));

    mask_data->addAtTime(TimeFrameIndex(50), makeMask(4), NotifyObservers::No);
    mask_data->addAtTime(TimeFrameIndex(1), makeMask(2), NotifyObservers::No);
    mask_data->addAtTime(TimeFrameIndex(2), makeMask(3), NotifyObservers::No);
    mask_data->addAtTime(TimeFrameIndex(3), makeMask(1), NotifyObservers::No);

    MaskTableModel model;
    model.setMasks(mask_data.get());

    REQUIRE(model.rowCount(QModelIndex{}) == 4);
    REQUIRE(model.getRowData(0).frame == 1);
    REQUIRE(model.getRowData(1).frame == 2);
    REQUIRE(model.getRowData(2).frame == 3);
    REQUIRE(model.getRowData(3).frame == 50);

    model.sort(0, Qt::DescendingOrder);
    REQUIRE(model.getRowData(0).frame == 50);
    REQUIRE(model.getRowData(3).frame == 1);

    model.sort(2, Qt::AscendingOrder);
    REQUIRE(model.getRowData(0).totalPointsInFrame == 1);
    REQUIRE(model.getRowData(3).totalPointsInFrame == 4);
}

TEST_CASE("LineTableModel sorts by length", "[InspectorTableSort][LineTableModel]") {
    ensureQCoreApplication();

    auto line_data = std::make_shared<LineData>();
    line_data->setTimeFrame(makeTimeFrame(100));

    line_data->addAtTime(TimeFrameIndex(0), makeLine(5), NotifyObservers::No);
    line_data->addAtTime(TimeFrameIndex(1), makeLine(2), NotifyObservers::No);
    line_data->addAtTime(TimeFrameIndex(2), makeLine(4), NotifyObservers::No);

    LineTableModel model;
    model.setLines(line_data.get());

    REQUIRE(model.getRowData(0).frame == 0);
    REQUIRE(model.getRowData(1).frame == 1);
    REQUIRE(model.getRowData(2).frame == 2);

    model.sort(2, Qt::AscendingOrder);
    REQUIRE(model.getRowData(0).length == 2);
    REQUIRE(model.getRowData(1).length == 4);
    REQUIRE(model.getRowData(2).length == 5);
}

TEST_CASE("PointTableModel sorts by X coordinate", "[InspectorTableSort][PointTableModel]") {
    ensureQCoreApplication();

    auto point_data = std::make_shared<PointData>();
    point_data->setTimeFrame(makeTimeFrame(100));

    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 0.0f}, NotifyObservers::No);
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 0.0f}, NotifyObservers::No);
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{20.0f, 0.0f}, NotifyObservers::No);

    PointTableModel model;
    model.setPoints(point_data.get());

    REQUIRE(model.getRowData(0).frame == 0);
    model.sort(2, Qt::AscendingOrder);

    REQUIRE(model.getRowData(0).x == 10.0f);
    REQUIRE(model.getRowData(1).x == 20.0f);
    REQUIRE(model.getRowData(2).x == 30.0f);
}

TEST_CASE("EventTableModel defaults to Frame ascending", "[InspectorTableSort][EventTableModel]") {
    ensureQCoreApplication();

    auto tf = makeTimeFrame(100);
    auto event_series = std::make_shared<DigitalEventSeries>();
    event_series->setTimeFrame(tf);
    event_series->addEvent(TimeFrameIndex(30));
    event_series->addEvent(TimeFrameIndex(10));
    event_series->addEvent(TimeFrameIndex(20));

    EventTableModel model;
    model.setEvents(event_series.get());

    REQUIRE(model.rowCount(QModelIndex{}) == 3);
    REQUIRE(model.getRowData(0).time.getValue() == 10);
    REQUIRE(model.getRowData(1).time.getValue() == 20);
    REQUIRE(model.getRowData(2).time.getValue() == 30);

    model.sort(0, Qt::DescendingOrder);
    REQUIRE(model.getRowData(0).time.getValue() == 30);
    REQUIRE(model.getRowData(2).time.getValue() == 10);
}

TEST_CASE("IntervalTableModel sorts by duration", "[InspectorTableSort][IntervalTableModel]") {
    ensureQCoreApplication();

    auto tf = makeTimeFrame(100);
    auto interval_series = std::make_shared<DigitalIntervalSeries>();
    interval_series->setTimeFrame(tf);
    interval_series->addEvent(TimeFrameIndex(0), TimeFrameIndex(9));  // duration 10
    interval_series->addEvent(TimeFrameIndex(20), TimeFrameIndex(22));// duration 3
    interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(34));// duration 5

    IntervalTableModel model;
    model.setIntervals(interval_series.get());

    REQUIRE(model.getRowData(0).interval.start.getValue() == 0);
    REQUIRE(model.getRowData(1).interval.start.getValue() == 20);
    REQUIRE(model.getRowData(2).interval.start.getValue() == 30);

    model.sort(2, Qt::AscendingOrder);

    REQUIRE(model.getRowData(0).interval.start.getValue() == 20);
    REQUIRE(model.getRowData(1).interval.start.getValue() == 30);
    REQUIRE(model.getRowData(2).interval.start.getValue() == 0);
}
