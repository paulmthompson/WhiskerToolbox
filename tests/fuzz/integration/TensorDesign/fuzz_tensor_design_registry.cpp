/**
 * @file fuzz_tensor_design_registry.cpp
 * @brief Fuzzes allowlisted scalar TensorDesign row-modifier and aggregator compositions.
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Points/Point_Data.hpp"
#include "TensorDesign/ColumnRecipePresetRegistry.hpp"
#include "TensorDesign/TensorDesignBuilder.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kRowCount = 3;
constexpr int64_t kMaxWindowTicks = 20;

struct RowWindowOffsets {
    int64_t pre = 0;
    int64_t post = 0;
};

struct ScalarCompositionCase {
    std::string_view modifier_id;
    std::string_view aggregator_id;
    bool fuzz_window = false;
};

// Scalar float tensor compositions only. Add cases incrementally as coverage grows.
//
// Tier 0 — full-interval gather, no row modifier (replaces mean/event_count/event_presence over interval).
// Tier 1 — interval_start + timestamp aggregators (replaces *_at_interval_start monolithic presets).
// Tier 2 — window_around_interval_start + interval reducers (replaces event_presence_around_interval_start).
// Tier 3 — bind_interval_start + trial_relative_event_count (replaces trial_relative_event_count_from_interval_start).
//
// Excluded until a ragged-column or dedicated raster test harness exists:
//   raster_events_relative — NormalizeDigitalEventSeriesRelative output stays DigitalEventSeries, not float.
constexpr std::array<ScalarCompositionCase, 10> kScalarCompositionCases{{
        // Tier 0
        {.modifier_id = {}, .aggregator_id = "mean_value"},
        {.modifier_id = {}, .aggregator_id = "event_count"},
        {.modifier_id = {}, .aggregator_id = "event_presence"},
        // Tier 1
        {.modifier_id = "interval_start", .aggregator_id = "analog_sample"},
        {.modifier_id = "interval_start", .aggregator_id = "point_xy"},
        {.modifier_id = "interval_start", .aggregator_id = "multi_point_xy"},
        // Tier 2
        {.modifier_id = "window_around_interval_start", .aggregator_id = "mean_value", .fuzz_window = true},
        {.modifier_id = "window_around_interval_start", .aggregator_id = "event_count", .fuzz_window = true},
        {.modifier_id = "window_around_interval_start", .aggregator_id = "event_presence", .fuzz_window = true},
        // Tier 3
        {.modifier_id = "bind_interval_start", .aggregator_id = "trial_relative_event_count"},
}};

static_assert(!kScalarCompositionCases.empty());

std::shared_ptr<TimeFrame> createIdentityTimeFrame() {
    std::vector<int> times;
    times.reserve(100);
    for (int time = 0; time < 100; ++time) {
        times.push_back(time);
    }
    return std::make_shared<TimeFrame>(std::move(times));
}

std::shared_ptr<AnalogTimeSeries> createAnalogSeries() {
    std::vector<float> values;
    std::vector<TimeFrameIndex> times;
    values.reserve(100);
    times.reserve(100);
    for (int64_t time = 0; time < 100; ++time) {
        values.push_back(static_cast<float>(time));
        times.emplace_back(time);
    }
    return std::make_shared<AnalogTimeSeries>(std::move(values), std::move(times));
}

std::shared_ptr<DigitalEventSeries> createEventSeries() {
    auto events = std::make_shared<DigitalEventSeries>();
    for (int64_t const time: {10, 12, 30, 33, 50, 54}) {
        events->addEvent(TimeFrameIndex(time));
    }
    return events;
}

std::shared_ptr<DigitalIntervalSeries> createIntervalSeries() {
    std::vector<TimeFrameInterval> intervals;
    intervals.emplace_back(TimeFrameIndex(10), TimeFrameIndex(20));
    intervals.emplace_back(TimeFrameIndex(30), TimeFrameIndex(40));
    intervals.emplace_back(TimeFrameIndex(50), TimeFrameIndex(60));
    return std::make_shared<DigitalIntervalSeries>(std::move(intervals));
}

std::shared_ptr<PointData> createPointSeries() {
    auto points = std::make_shared<PointData>();
    for (int64_t time = 0; time < 100; ++time) {
        points->addAtTime(
                TimeFrameIndex(time),
                Point2D<float>{static_cast<float>(time), static_cast<float>(-time)},
                NotifyObservers::No);
    }
    return points;
}

void populateDataManager(DataManager & manager) {
    static_cast<void>(manager.setTime(TimeKey("time"), createIdentityTimeFrame(), true));
    manager.setData<AnalogTimeSeries>("analog", createAnalogSeries(), TimeKey("time"));
    manager.setData<DigitalEventSeries>("events", createEventSeries(), TimeKey("time"));
    manager.setData<DigitalIntervalSeries>("intervals", createIntervalSeries(), TimeKey("time"));
    manager.setData<PointData>("points", createPointSeries(), TimeKey("time"));
}

Neuralyzer::TensorDesign::ColumnRecipePresetArgs createArgs(
        std::string_view aggregator_id,
        RowWindowOffsets window) {
    Neuralyzer::TensorDesign::ColumnRecipePresetArgs args;
    args.output_name = "feature";
    args.pre = window.pre;
    args.post = window.post;
    args.binding_source_key = "intervals";
    args.store_key = "interval_start";
    args.window_start = 0.0;
    args.window_end = 10.0;

    if (aggregator_id == "mean_value" || aggregator_id == "analog_sample") {
        args.source_key = "analog";
    } else if (aggregator_id == "point_xy") {
        args.source_key = "points";
        args.name_prefix = "point";
    } else if (aggregator_id == "multi_point_xy") {
        args.source_keys = {"points"};
    } else {
        args.source_key = "events";
    }
    return args;
}

[[nodiscard]] Neuralyzer::TensorDesign::RowModifierDescriptor const * findModifier(
        Neuralyzer::TensorDesign::RowModifierRegistry const & registry,
        std::string_view modifier_id) {
    if (modifier_id.empty()) {
        return nullptr;
    }
    return registry.find(std::string(modifier_id));
}

[[nodiscard]] Neuralyzer::TensorDesign::ColumnAggregatorDescriptor const * findAggregator(
        Neuralyzer::TensorDesign::ColumnAggregatorRegistry const & registry,
        std::string_view aggregator_id) {
    return registry.find(std::string(aggregator_id));
}

void FuzzScalarPresetComposition(uint16_t encoded_input) {
    auto const case_index = static_cast<uint8_t>(encoded_input % kScalarCompositionCases.size());
    auto const window_seed = static_cast<int64_t>(encoded_input / kScalarCompositionCases.size());
    auto const & composition_case = kScalarCompositionCases[case_index];

    auto modifier_registry = Neuralyzer::TensorDesign::createBuiltInRowModifierRegistry();
    auto aggregator_registry = Neuralyzer::TensorDesign::createBuiltInColumnAggregatorRegistry();

    auto const * modifier = findModifier(modifier_registry, composition_case.modifier_id);
    if (!composition_case.modifier_id.empty()) {
        ASSERT_NE(modifier, nullptr);
    }

    auto const * aggregator = findAggregator(aggregator_registry, composition_case.aggregator_id);
    ASSERT_NE(aggregator, nullptr);

    RowWindowOffsets const window{
            .pre = composition_case.fuzz_window ? window_seed % (kMaxWindowTicks + 1) : 0,
            .post = composition_case.fuzz_window ? (window_seed / (kMaxWindowTicks + 1)) % (kMaxWindowTicks + 1)
                                                 : 0};
    auto args = createArgs(composition_case.aggregator_id, window);

    auto aggregator_expansion = aggregator->expand(args);
    if (!aggregator_expansion.has_value()) {
        FAIL() << "aggregator expansion failed for " << composition_case.aggregator_id;
    }
    if (aggregator_expansion->columns.empty()) {
        FAIL() << "aggregator expansion produced no columns for " << composition_case.aggregator_id;
    }

    auto recipes = std::move(aggregator_expansion->columns);
    if (modifier != nullptr) {
        auto modifier_expansion = modifier->expand(args);
        if (!modifier_expansion.has_value()) {
            FAIL() << "modifier expansion failed for " << composition_case.modifier_id;
        }
        auto const & modifier_recipe = *modifier_expansion;
        for (auto & recipe: recipes) {
            recipe.row_pipeline_json = modifier_recipe.row_pipeline_json;
            recipe.pipeline_value_bindings = modifier_recipe.pipeline_value_bindings;
        }
    }

    auto const column_count = recipes.size();
    Neuralyzer::TensorDesign::TensorDesignSpec const design{
            .tensor_key = "fuzz_tensor",
            .row_source_key = "intervals",
            .row_type = Neuralyzer::TensorDesign::RowType::Interval,
            .columns = std::move(recipes)};

    DataManager manager;
    populateDataManager(manager);
    if (!Neuralyzer::TensorDesign::populateDataManager(manager, design)) {
        FAIL() << "populateDataManager failed for modifier=" << composition_case.modifier_id
               << " aggregator=" << composition_case.aggregator_id;
    }
    auto tensor = manager.getData<TensorData>("fuzz_tensor");
    ASSERT_NE(tensor, nullptr);
    EXPECT_EQ(tensor->numRows(), kRowCount);
    EXPECT_EQ(tensor->numColumns(), column_count);
    EXPECT_EQ(manager.getTimeKey("fuzz_tensor").str(), "time");
}

FUZZ_TEST(TensorDesignRegistryFuzz, FuzzScalarPresetComposition)
        .WithDomains(fuzztest::InRange<uint16_t>(
                0,
                static_cast<uint16_t>(
                        kScalarCompositionCases.size() * (kMaxWindowTicks + 1) * (kMaxWindowTicks + 1) - 1)));

}// namespace
