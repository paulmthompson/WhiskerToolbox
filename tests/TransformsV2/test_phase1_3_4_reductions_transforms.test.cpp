/**
 * @file test_phase1_3_4_reductions_transforms.test.cpp
 * @brief Builder-scenario tests for Phase 1.3 (Range Reductions) and
 *        Phase 1.4 (Element Transforms / Builder Extensions).
 *
 * Each test constructs a scenario with known inputs and validates against
 * expected outputs — the same values and expectations that would be produced
 * by the (soon-to-be-deprecated) TableView computers.
 *
 * Test organisation:
 *   Section A: IntervalRangeReductions (standalone reduction function tests)
 *   Section B: EventPresence reduction
 *   Section C: LineLength element transform
 *   Section D: AnalogSampleAtOffset builder function
 *   Section E: End-to-end builder scenarios (reductions through TensorColumnBuilders)
 *
 * @see IntervalRangeReductions.hpp
 * @see EventRangeReductions.hpp  (eventPresence)
 * @see LineLength.hpp
 * @see TensorColumnBuilders.hpp  (buildAnalogSampleAtOffsetProvider)
 */

#include "TransformsV2/algorithms/RangeReductions/IntervalRangeReductions.hpp"
#include "TransformsV2/algorithms/RangeReductions/EventRangeReductions.hpp"
#include "TransformsV2/algorithms/LineLength/LineLength.hpp"

#include "TransformsV2/core/TensorColumnBuilders.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"
#include "DataManager/Tensors/storage/LazyColumnTensorStorage.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <any>
#include <cmath>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2::RangeReductions;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using namespace WhiskerToolbox::TensorBuilders;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

std::shared_ptr<AnalogTimeSeries> createLinearAnalog(std::size_t num_samples) {
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(num_samples);
    times.reserve(num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
        data.push_back(static_cast<float>(i));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    return std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
}

std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
    std::vector<std::pair<int64_t, int64_t>> const & intervals)
{
    std::vector<Interval> vec;
    vec.reserve(intervals.size());
    for (auto const & [s, e] : intervals) {
        vec.push_back(Interval{s, e});
    }
    return std::make_shared<DigitalIntervalSeries>(vec);
}

std::shared_ptr<DigitalEventSeries> createEventSeries(
    std::vector<int64_t> const & times)
{
    auto series = std::make_shared<DigitalEventSeries>();
    for (auto t : times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

std::unique_ptr<DataManager> makeDMWithAnalog(std::string const & key, std::size_t samples) {
    auto dm = std::make_unique<DataManager>();
    auto analog = createLinearAnalog(samples);
    dm->setData<AnalogTimeSeries>(key, analog, TimeKey("time"));
    return dm;
}

std::vector<TimeFrameIndex> makeRowTimes(std::vector<int64_t> const & ts) {
    std::vector<TimeFrameIndex> result;
    result.reserve(ts.size());
    for (auto t : ts) {
        result.push_back(TimeFrameIndex(t));
    }
    return result;
}

} // anonymous namespace

// =============================================================================
// Section A: IntervalRangeReductions — standalone function tests
// =============================================================================

TEST_CASE("IntervalCount — counts intervals in span", "[Phase1.3][IntervalReductions]") {
    // Build a span of 3 IntervalWithId elements
    std::vector<IntervalWithId> data{
        IntervalWithId{Interval{10, 20}, EntityId{0}},
        IntervalWithId{Interval{30, 50}, EntityId{1}},
        IntervalWithId{Interval{60, 90}, EntityId{2}}
    };
    std::span<IntervalWithId const> span{data};

    CHECK(intervalCount(span) == 3);
}

TEST_CASE("IntervalCount — empty span returns zero", "[Phase1.3][IntervalReductions]") {
    std::span<IntervalWithId const> empty;
    CHECK(intervalCount(empty) == 0);
}

TEST_CASE("IntervalStartExtract — returns start of first interval", "[Phase1.3][IntervalReductions]") {
    std::vector<IntervalWithId> data{
        IntervalWithId{Interval{100, 200}, EntityId{0}},
        IntervalWithId{Interval{300, 400}, EntityId{1}}
    };
    std::span<IntervalWithId const> span{data};

    CHECK_THAT(intervalStartExtract(span), WithinAbs(100.0f, 1e-5));
}

TEST_CASE("IntervalStartExtract — empty span returns NaN", "[Phase1.3][IntervalReductions]") {
    std::span<IntervalWithId const> empty;
    CHECK(std::isnan(intervalStartExtract(empty)));
}

TEST_CASE("IntervalEndExtract — returns end of first interval", "[Phase1.3][IntervalReductions]") {
    std::vector<IntervalWithId> data{
        IntervalWithId{Interval{100, 200}, EntityId{0}},
        IntervalWithId{Interval{300, 400}, EntityId{1}}
    };
    std::span<IntervalWithId const> span{data};

    CHECK_THAT(intervalEndExtract(span), WithinAbs(200.0f, 1e-5));
}

TEST_CASE("IntervalEndExtract — empty span returns NaN", "[Phase1.3][IntervalReductions]") {
    std::span<IntervalWithId const> empty;
    CHECK(std::isnan(intervalEndExtract(empty)));
}

TEST_CASE("IntervalSourceIndex — returns entity ID of first interval", "[Phase1.3][IntervalReductions]") {
    std::vector<IntervalWithId> data{
        IntervalWithId{Interval{10, 20}, EntityId{42}},
        IntervalWithId{Interval{30, 50}, EntityId{99}}
    };
    std::span<IntervalWithId const> span{data};

    CHECK(intervalSourceIndex(span) == 42);
}

TEST_CASE("IntervalSourceIndex — empty span returns -1", "[Phase1.3][IntervalReductions]") {
    std::span<IntervalWithId const> empty;
    CHECK(intervalSourceIndex(empty) == -1);
}

TEST_CASE("IntervalCount — single interval", "[Phase1.3][IntervalReductions]") {
    std::vector<IntervalWithId> data{
        IntervalWithId{Interval{0, 100}, EntityId{5}}
    };
    std::span<IntervalWithId const> span{data};

    CHECK(intervalCount(span) == 1);
    CHECK_THAT(intervalStartExtract(span), WithinAbs(0.0f, 1e-5));
    CHECK_THAT(intervalEndExtract(span), WithinAbs(100.0f, 1e-5));
    CHECK(intervalSourceIndex(span) == 5);
}

// =============================================================================
// Section B: EventPresence reduction
// =============================================================================

TEST_CASE("EventPresence — returns 1 when events exist", "[Phase1.3][EventPresence]") {
    std::vector<EventWithId> data{
        EventWithId{TimeFrameIndex(10), EntityId{0}},
        EventWithId{TimeFrameIndex(20), EntityId{0}}
    };
    std::span<EventWithId const> span{data};

    CHECK(eventPresence(span) == 1);
}

TEST_CASE("EventPresence — returns 0 when no events", "[Phase1.3][EventPresence]") {
    std::span<EventWithId const> empty;
    CHECK(eventPresence(empty) == 0);
}

TEST_CASE("EventPresence — single event returns 1", "[Phase1.3][EventPresence]") {
    std::vector<EventWithId> data{
        EventWithId{TimeFrameIndex(5), EntityId{0}}
    };
    std::span<EventWithId const> span{data};

    CHECK(eventPresence(span) == 1);
}

// =============================================================================
// Section C: LineLength element transform
// =============================================================================

TEST_CASE("LineLength — horizontal line", "[Phase1.4][LineLength]") {
    // Line from (0,0) to (10,0) — arc length = 10
    Line2D line({Point2D<float>{0.0f, 0.0f}, Point2D<float>{10.0f, 0.0f}});
    LineLengthParams params;

    CHECK_THAT(calculateLineLength(line, params), WithinAbs(10.0f, 1e-5));
}

TEST_CASE("LineLength — vertical line", "[Phase1.4][LineLength]") {
    // Line from (0,0) to (0,5) — arc length = 5
    Line2D line({Point2D<float>{0.0f, 0.0f}, Point2D<float>{0.0f, 5.0f}});
    LineLengthParams params;

    CHECK_THAT(calculateLineLength(line, params), WithinAbs(5.0f, 1e-5));
}

TEST_CASE("LineLength — multi-segment line", "[Phase1.4][LineLength]") {
    // Right angle path: (0,0)→(3,0)→(3,4) — arc length = 3 + 4 = 7
    Line2D line({
        Point2D<float>{0.0f, 0.0f},
        Point2D<float>{3.0f, 0.0f},
        Point2D<float>{3.0f, 4.0f}
    });
    LineLengthParams params;

    CHECK_THAT(calculateLineLength(line, params), WithinAbs(7.0f, 1e-5));
}

TEST_CASE("LineLength — diagonal 3-4-5 triangle", "[Phase1.4][LineLength]") {
    // Single segment (0,0)→(3,4) — arc length = 5
    Line2D line({Point2D<float>{0.0f, 0.0f}, Point2D<float>{3.0f, 4.0f}});
    LineLengthParams params;

    CHECK_THAT(calculateLineLength(line, params), WithinAbs(5.0f, 1e-5));
}

TEST_CASE("LineLength — single point returns 0", "[Phase1.4][LineLength]") {
    Line2D line({Point2D<float>{1.0f, 2.0f}});
    LineLengthParams params;

    CHECK_THAT(calculateLineLength(line, params), WithinAbs(0.0f, 1e-5));
}

TEST_CASE("LineLength — empty line returns 0", "[Phase1.4][LineLength]") {
    Line2D line;
    LineLengthParams params;

    CHECK_THAT(calculateLineLength(line, params), WithinAbs(0.0f, 1e-5));
}

// =============================================================================
// Section D: AnalogSampleAtOffset builder
// =============================================================================

TEST_CASE("buildAnalogSampleAtOffsetProvider — positive offset", "[Phase1.4][AnalogSampleAtOffset]") {
    // Analog data: values = [0, 1, 2, ..., 99]
    auto dm = makeDMWithAnalog("analog", 100);
    auto row_times = makeRowTimes({10, 20, 30});

    // Offset +5: reads at times 15, 25, 35
    auto provider = buildAnalogSampleAtOffsetProvider(*dm, "analog", row_times, 5);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(15.0f, 1e-5));
    CHECK_THAT(values[1], WithinAbs(25.0f, 1e-5));
    CHECK_THAT(values[2], WithinAbs(35.0f, 1e-5));
}

TEST_CASE("buildAnalogSampleAtOffsetProvider — negative offset", "[Phase1.4][AnalogSampleAtOffset]") {
    auto dm = makeDMWithAnalog("analog", 100);
    auto row_times = makeRowTimes({10, 20, 30});

    // Offset -3: reads at times 7, 17, 27
    auto provider = buildAnalogSampleAtOffsetProvider(*dm, "analog", row_times, -3);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(7.0f, 1e-5));
    CHECK_THAT(values[1], WithinAbs(17.0f, 1e-5));
    CHECK_THAT(values[2], WithinAbs(27.0f, 1e-5));
}

TEST_CASE("buildAnalogSampleAtOffsetProvider — offset out of range produces NaN", "[Phase1.4][AnalogSampleAtOffset]") {
    auto dm = makeDMWithAnalog("analog", 10); // times 0..9
    auto row_times = makeRowTimes({0, 5, 8});

    // Offset +10: reads at times 10, 15, 18 — all out of range for 10-sample source
    auto provider = buildAnalogSampleAtOffsetProvider(*dm, "analog", row_times, 10);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK(std::isnan(values[0]));
    CHECK(std::isnan(values[1]));
    CHECK(std::isnan(values[2]));
}

TEST_CASE("buildAnalogSampleAtOffsetProvider — zero offset same as direct", "[Phase1.4][AnalogSampleAtOffset]") {
    auto dm = makeDMWithAnalog("analog", 100);
    auto row_times = makeRowTimes({0, 50, 99});

    auto provider_offset = buildAnalogSampleAtOffsetProvider(*dm, "analog", row_times, 0);
    auto provider_direct = buildDirectColumnProvider(*dm, "analog", row_times);

    auto vals_offset = provider_offset();
    auto vals_direct = provider_direct();

    REQUIRE(vals_offset.size() == vals_direct.size());
    for (std::size_t i = 0; i < vals_offset.size(); ++i) {
        CHECK_THAT(vals_offset[i], WithinAbs(vals_direct[i], 1e-5));
    }
}

TEST_CASE("buildAnalogSampleAtOffsetProvider — invalid source throws", "[Phase1.4][AnalogSampleAtOffset]") {
    DataManager dm;
    auto row_times = makeRowTimes({0});

    CHECK_THROWS_AS(
        buildAnalogSampleAtOffsetProvider(dm, "nonexistent", row_times, 0),
        std::runtime_error);
}

TEST_CASE("buildAnalogSampleAtOffsetProvider — multi-column offset scenario", "[Phase1.4][AnalogSampleAtOffset]") {
    // Scenario: sample at offsets -2, 0, +2 for each row time
    // This replaces the old AnalogTimestampOffsetsMultiComputer
    auto dm = makeDMWithAnalog("analog", 100);
    auto row_times = makeRowTimes({10, 20, 30});

    auto p_minus2 = buildAnalogSampleAtOffsetProvider(*dm, "analog", row_times, -2);
    auto p_zero   = buildAnalogSampleAtOffsetProvider(*dm, "analog", row_times, 0);
    auto p_plus2  = buildAnalogSampleAtOffsetProvider(*dm, "analog", row_times, 2);

    auto v_minus2 = p_minus2();
    auto v_zero   = p_zero();
    auto v_plus2  = p_plus2();

    // Row 0: reads at 8, 10, 12
    CHECK_THAT(v_minus2[0], WithinAbs(8.0f, 1e-5));
    CHECK_THAT(v_zero[0],   WithinAbs(10.0f, 1e-5));
    CHECK_THAT(v_plus2[0],  WithinAbs(12.0f, 1e-5));

    // Row 1: reads at 18, 20, 22
    CHECK_THAT(v_minus2[1], WithinAbs(18.0f, 1e-5));
    CHECK_THAT(v_zero[1],   WithinAbs(20.0f, 1e-5));
    CHECK_THAT(v_plus2[1],  WithinAbs(22.0f, 1e-5));
}

// =============================================================================
// Section E: End-to-end builder scenarios with new reductions
// =============================================================================

TEST_CASE("Builder scenario — IntervalCount through buildIntervalReductionProvider",
          "[Phase1.3][BuilderScenario]") {
    // Source: interval series with 3 intervals
    // Row intervals: 2 rows
    //   Row 0 [0, 50):  overlaps source intervals at [10,20] and [30,40]
    //   Row 1 [60, 100): overlaps source interval at [70,80]
    auto dm = std::make_unique<DataManager>();

    auto source_intervals = createIntervalSeries({{10, 20}, {30, 40}, {70, 80}});
    dm->setData<DigitalIntervalSeries>("source_ivals", source_intervals, TimeKey("time"));

    auto row_intervals = createIntervalSeries({{0, 50}, {60, 100}});

    // Pipeline with IntervalCount reduction
    WhiskerToolbox::Transforms::V2::TransformPipeline pipeline;
    pipeline.setRangeReductionErased("IntervalCount", std::any{});

    auto provider = buildIntervalReductionProvider(
        *dm, "source_ivals", row_intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 2);
    // Row 0 has 2 overlapping intervals, Row 1 has 1
    CHECK_THAT(values[0], WithinAbs(2.0f, 1e-5));
    CHECK_THAT(values[1], WithinAbs(1.0f, 1e-5));
}

TEST_CASE("Builder scenario — IntervalStartExtract through builder",
          "[Phase1.3][BuilderScenario]") {
    auto dm = std::make_unique<DataManager>();

    auto source_intervals = createIntervalSeries({{100, 200}, {300, 400}});
    dm->setData<DigitalIntervalSeries>("source_ivals", source_intervals, TimeKey("time"));

    // Row interval [50, 250) overlaps source [100, 200]
    auto row_intervals = createIntervalSeries({{50, 250}});

    WhiskerToolbox::Transforms::V2::TransformPipeline pipeline;
    pipeline.setRangeReductionErased("IntervalStartExtract", std::any{});

    auto provider = buildIntervalReductionProvider(
        *dm, "source_ivals", row_intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 1);
    // First overlapping interval starts at 100
    CHECK_THAT(values[0], WithinAbs(100.0f, 1e-5));
}

TEST_CASE("Builder scenario — IntervalEndExtract through builder",
          "[Phase1.3][BuilderScenario]") {
    auto dm = std::make_unique<DataManager>();

    auto source_intervals = createIntervalSeries({{100, 200}, {300, 400}});
    dm->setData<DigitalIntervalSeries>("source_ivals", source_intervals, TimeKey("time"));

    auto row_intervals = createIntervalSeries({{50, 250}});

    WhiskerToolbox::Transforms::V2::TransformPipeline pipeline;
    pipeline.setRangeReductionErased("IntervalEndExtract", std::any{});

    auto provider = buildIntervalReductionProvider(
        *dm, "source_ivals", row_intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 1);
    // First overlapping interval ends at 200
    CHECK_THAT(values[0], WithinAbs(200.0f, 1e-5));
}

TEST_CASE("Builder scenario — EventPresence through builder",
          "[Phase1.3][BuilderScenario]") {
    auto dm = std::make_unique<DataManager>();

    // Events at times 15, 25, 75
    auto events = createEventSeries({15, 25, 75});
    dm->setData<DigitalEventSeries>("spikes", events, TimeKey("time"));

    // Row 0: [0, 50) has events at 15, 25 → presence = 1
    // Row 1: [50, 60) has no events     → presence = 0
    // Row 2: [60, 100) has event at 75  → presence = 1
    auto row_intervals = createIntervalSeries({{0, 50}, {50, 60}, {60, 100}});

    WhiskerToolbox::Transforms::V2::TransformPipeline pipeline;
    pipeline.setRangeReductionErased("EventPresence", std::any{});

    auto provider = buildIntervalReductionProvider(
        *dm, "spikes", row_intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(1.0f, 1e-5));  // has events
    CHECK_THAT(values[1], WithinAbs(0.0f, 1e-5));  // no events
    CHECK_THAT(values[2], WithinAbs(1.0f, 1e-5));  // has event
}

TEST_CASE("Builder scenario — IntervalSourceIndex through builder",
          "[Phase1.3][BuilderScenario]") {
    auto dm = std::make_unique<DataManager>();

    // Source intervals all have EntityId{0} (default constructor from vector<Interval>)
    auto source_intervals = createIntervalSeries({{100, 200}, {300, 400}});
    dm->setData<DigitalIntervalSeries>("source_ivals", source_intervals, TimeKey("time"));

    auto row_intervals = createIntervalSeries({{50, 250}});

    WhiskerToolbox::Transforms::V2::TransformPipeline pipeline;
    pipeline.setRangeReductionErased("IntervalSourceIndex", std::any{});

    auto provider = buildIntervalReductionProvider(
        *dm, "source_ivals", row_intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 1);
    // After setData, DataManager::rebuildAllEntityIds() reassigns IDs starting from 1
    // First overlapping interval gets EntityId{1}
    CHECK_THAT(values[0], WithinAbs(1.0f, 1e-5));
}

TEST_CASE("Builder scenario — empty interval overlap produces default values",
          "[Phase1.3][BuilderScenario]") {
    auto dm = std::make_unique<DataManager>();

    auto source_intervals = createIntervalSeries({{100, 200}});
    dm->setData<DigitalIntervalSeries>("source_ivals", source_intervals, TimeKey("time"));

    // Row interval [0, 50) doesn't overlap any source intervals
    auto row_intervals = createIntervalSeries({{0, 50}});

    // IntervalCount on empty gather should be 0
    {
        WhiskerToolbox::Transforms::V2::TransformPipeline pipeline;
        pipeline.setRangeReductionErased("IntervalCount", std::any{});
        auto provider = buildIntervalReductionProvider(
            *dm, "source_ivals", row_intervals, std::move(pipeline));
        auto values = provider();
        REQUIRE(values.size() == 1);
        CHECK_THAT(values[0], WithinAbs(0.0f, 1e-5));
    }

    // IntervalStartExtract on empty gather should be NaN
    {
        WhiskerToolbox::Transforms::V2::TransformPipeline pipeline;
        pipeline.setRangeReductionErased("IntervalStartExtract", std::any{});
        auto provider = buildIntervalReductionProvider(
            *dm, "source_ivals", row_intervals, std::move(pipeline));
        auto values = provider();
        REQUIRE(values.size() == 1);
        CHECK(std::isnan(values[0]));
    }
}

TEST_CASE("Builder scenario — multi-column tensor with new reductions",
          "[Phase1.3][BuilderScenario]") {
    // Scenario: Analog source [0..99], event source at {5, 15, 55}
    //           2 row intervals: [0, 30), [40, 70)
    // Columns: Mean of analog, event count, event presence
    auto dm = std::make_unique<DataManager>();
    auto analog = createLinearAnalog(100);
    dm->setData<AnalogTimeSeries>("analog", analog, TimeKey("time"));
    auto events = createEventSeries({5, 15, 55});
    dm->setData<DigitalEventSeries>("events", events, TimeKey("time"));

    auto row_intervals = createIntervalSeries({{0, 30}, {40, 70}});

    // Column 1: MeanValue of analog
    WhiskerToolbox::Transforms::V2::TransformPipeline p1;
    p1.setRangeReductionErased("MeanValue", std::any{});
    auto prov1 = buildIntervalReductionProvider(*dm, "analog", row_intervals, std::move(p1));

    // Column 2: EventCount
    WhiskerToolbox::Transforms::V2::TransformPipeline p2;
    p2.setRangeReductionErased("EventCount", std::any{});
    auto prov2 = buildIntervalReductionProvider(*dm, "events", row_intervals, std::move(p2));

    // Column 3: EventPresence
    WhiskerToolbox::Transforms::V2::TransformPipeline p3;
    p3.setRangeReductionErased("EventPresence", std::any{});
    auto prov3 = buildIntervalReductionProvider(*dm, "events", row_intervals, std::move(p3));

    // Build lazy tensor
    std::vector<ColumnSource> columns{
        ColumnSource{"mean_analog", std::move(prov1)},
        ColumnSource{"event_count", std::move(prov2)},
        ColumnSource{"event_presence", std::move(prov3)}
    };

    auto tensor = TensorData::createFromLazyColumns(
        2, std::move(columns), RowDescriptor::ordinal(2), nullptr);

    // Row 0 [0,30): analog mean of 0..29 = 14.5, events {5,15} = count 2, presence 1
    // Row 1 [40,70): analog mean of 40..69 = 54.5, events {55} = count 1, presence 1
    auto mean_col = tensor.getColumn(0);
    auto count_col = tensor.getColumn(1);
    auto pres_col = tensor.getColumn(2);

    CHECK_THAT(mean_col[0], WithinAbs(14.5f, 0.5f));
    CHECK_THAT(mean_col[1], WithinAbs(54.5f, 0.5f));
    CHECK_THAT(count_col[0], WithinAbs(2.0f, 1e-5));
    CHECK_THAT(count_col[1], WithinAbs(1.0f, 1e-5));
    CHECK_THAT(pres_col[0], WithinAbs(1.0f, 1e-5));
    CHECK_THAT(pres_col[1], WithinAbs(1.0f, 1e-5));
}

TEST_CASE("Builder scenario — offset columns as lazy tensor",
          "[Phase1.4][BuilderScenario]") {
    // Scenario: Multi-column offset sampling replacing AnalogTimestampOffsetsMultiComputer
    // Source: linear analog [0..99]
    // Row times: 20, 40, 60
    // Three columns at offsets: -5, 0, +5
    auto dm = makeDMWithAnalog("src", 100);
    auto row_times = makeRowTimes({20, 40, 60});

    auto col_m5 = buildAnalogSampleAtOffsetProvider(*dm, "src", row_times, -5);
    auto col_0  = buildAnalogSampleAtOffsetProvider(*dm, "src", row_times,  0);
    auto col_p5 = buildAnalogSampleAtOffsetProvider(*dm, "src", row_times,  5);

    std::vector<ColumnSource> columns{
        ColumnSource{"t-5", std::move(col_m5)},
        ColumnSource{"t+0", std::move(col_0)},
        ColumnSource{"t+5", std::move(col_p5)}
    };

    auto tensor = TensorData::createFromLazyColumns(
        3, std::move(columns), RowDescriptor::ordinal(3), nullptr);

    // Row 0 (t=20): 15.0, 20.0, 25.0
    // Row 1 (t=40): 35.0, 40.0, 45.0
    // Row 2 (t=60): 55.0, 60.0, 65.0
    CHECK_THAT(tensor.getColumn(0)[0], WithinAbs(15.0f, 1e-5));
    CHECK_THAT(tensor.getColumn(1)[0], WithinAbs(20.0f, 1e-5));
    CHECK_THAT(tensor.getColumn(2)[0], WithinAbs(25.0f, 1e-5));

    CHECK_THAT(tensor.getColumn(0)[1], WithinAbs(35.0f, 1e-5));
    CHECK_THAT(tensor.getColumn(1)[1], WithinAbs(40.0f, 1e-5));
    CHECK_THAT(tensor.getColumn(2)[1], WithinAbs(45.0f, 1e-5));

    CHECK_THAT(tensor.getColumn(0)[2], WithinAbs(55.0f, 1e-5));
    CHECK_THAT(tensor.getColumn(1)[2], WithinAbs(60.0f, 1e-5));
    CHECK_THAT(tensor.getColumn(2)[2], WithinAbs(65.0f, 1e-5));
}

// =============================================================================
// Section F: Registry integration — verify new reductions are discoverable
// =============================================================================

TEST_CASE("RangeReductionRegistry — IntervalCount is registered", "[Phase1.3][Registry]") {
    auto & registry = WhiskerToolbox::Transforms::V2::RangeReductionRegistry::instance();
    CHECK(registry.hasReduction("IntervalCount"));
}

TEST_CASE("RangeReductionRegistry — IntervalStartExtract is registered", "[Phase1.3][Registry]") {
    auto & registry = WhiskerToolbox::Transforms::V2::RangeReductionRegistry::instance();
    CHECK(registry.hasReduction("IntervalStartExtract"));
}

TEST_CASE("RangeReductionRegistry — IntervalEndExtract is registered", "[Phase1.3][Registry]") {
    auto & registry = WhiskerToolbox::Transforms::V2::RangeReductionRegistry::instance();
    CHECK(registry.hasReduction("IntervalEndExtract"));
}

TEST_CASE("RangeReductionRegistry — IntervalSourceIndex is registered", "[Phase1.3][Registry]") {
    auto & registry = WhiskerToolbox::Transforms::V2::RangeReductionRegistry::instance();
    CHECK(registry.hasReduction("IntervalSourceIndex"));
}

TEST_CASE("RangeReductionRegistry — EventPresence is registered", "[Phase1.3][Registry]") {
    auto & registry = WhiskerToolbox::Transforms::V2::RangeReductionRegistry::instance();
    CHECK(registry.hasReduction("EventPresence"));
}

TEST_CASE("ElementRegistry — CalculateLineLength is registered", "[Phase1.4][Registry]") {
    auto & registry = WhiskerToolbox::Transforms::V2::ElementRegistry::instance();
    CHECK(registry.hasTransform("CalculateLineLength"));
}
