/**
 * @file DataManagerTemporalSubset.test.cpp
 * @brief Unit tests for createTemporalSubset() utility.
 */

#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/masks.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Tensors/TensorData.hpp"
#include "utils/DataManagerTemporalSubset.hpp"

#include <memory>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

namespace {

[[nodiscard]] std::unique_ptr<DataManager> makeDataManager() {
    return std::make_unique<DataManager>();
}

template<typename T>
[[nodiscard]] std::shared_ptr<T> requireSubsetPtr(std::optional<DataTypeVariant> const & result) {
    REQUIRE(result.has_value());
    auto const & ptr = std::get<std::shared_ptr<T>>(result.value());
    REQUIRE(ptr != nullptr);
    return ptr;
}

}// namespace

TEST_CASE("createTemporalSubset - AnalogTimeSeries view aliases source", "[TemporalSubset][Analog]") {
    auto const values = std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto const times = std::vector<TimeFrameIndex>{
            TimeFrameIndex{0},
            TimeFrameIndex{10},
            TimeFrameIndex{20},
            TimeFrameIndex{30},
            TimeFrameIndex{40}};

    auto source = std::make_shared<AnalogTimeSeries>(values, times);
    DataTypeVariant const input{source};

    std::string error;
    auto const result = createTemporalSubset(
            input,
            TimeFrameInterval{TimeFrameIndex{10}, TimeFrameIndex{30}},
            error);

    auto const view = requireSubsetPtr<AnalogTimeSeries>(result);
    REQUIRE(error.empty());
    REQUIRE(view->getNumSamples() == 3);
    auto const view_span = view->getAnalogTimeSeries();
    auto const source_span = source->getAnalogTimeSeries();
    REQUIRE(view_span.data() == source_span.data() + 1);
    REQUIRE(view_span[0] == 2.0f);
    REQUIRE(view_span[2] == 4.0f);
}

TEST_CASE("createTemporalSubset - DigitalEventSeries event count", "[TemporalSubset][DigitalEvent]") {
    auto source = std::make_shared<DigitalEventSeries>(std::vector<TimeFrameIndex>{
            TimeFrameIndex{5},
            TimeFrameIndex{15},
            TimeFrameIndex{25},
            TimeFrameIndex{35}});

    DataTypeVariant const input{source};
    std::string error;
    auto const result = createTemporalSubset(
            input,
            TimeFrameInterval{TimeFrameIndex{10}, TimeFrameIndex{30}},
            error);

    auto const view = requireSubsetPtr<DigitalEventSeries>(result);
    REQUIRE(error.empty());
    REQUIRE(view->size() == 2);
}

TEST_CASE("createTemporalSubset - MaskData zero-copy payload", "[TemporalSubset][Mask]") {
    Mask2D const mask_a{{1, 1}};
    Mask2D const mask_b{{3, 4}};
    Mask2D const mask_c{{5, 6}};

    auto source = std::make_shared<MaskData>();
    source->addAtTime(TimeFrameIndex{10}, mask_a, NotifyObservers::No);
    source->addAtTime(TimeFrameIndex{20}, mask_b, NotifyObservers::No);
    source->addAtTime(TimeFrameIndex{30}, mask_c, NotifyObservers::No);

    auto const source_at_20 = source->getAtTime(TimeFrameIndex{20});
    REQUIRE(std::ranges::distance(source_at_20) == 1);
    Mask2D const & stored_mask_b = *std::ranges::begin(source_at_20);

    DataTypeVariant const input{source};
    std::string error;
    auto const result = createTemporalSubset(
            input,
            TimeFrameInterval{TimeFrameIndex{15}, TimeFrameIndex{25}},
            error);

    auto const subset = requireSubsetPtr<MaskData>(result);
    REQUIRE(error.empty());
    REQUIRE(std::ranges::distance(subset->getAtTime(TimeFrameIndex{10})) == 0);

    auto const subset_at_20 = subset->getAtTime(TimeFrameIndex{20});
    REQUIRE(std::ranges::distance(subset_at_20) == 1);

    Mask2D const & view_mask = *std::ranges::begin(subset_at_20);
    REQUIRE(subset->isLazy());
    REQUIRE(view_mask.size() == stored_mask_b.size());
    REQUIRE(view_mask[0].x == stored_mask_b[0].x);
    REQUIRE(view_mask[0].y == stored_mask_b[0].y);
}

TEST_CASE("createTemporalSubset - unsupported and error paths", "[TemporalSubset][Errors]") {
    std::string error;

    SECTION("null shared_ptr in variant") {
        DataTypeVariant const input{std::shared_ptr<AnalogTimeSeries>{}};
        auto const result = createTemporalSubset(
                input,
                TimeFrameIndex{0},
                error);
        REQUIRE_FALSE(result.has_value());
        REQUIRE_FALSE(error.empty());
    }

    SECTION("invalid interval") {
        auto source = std::make_shared<AnalogTimeSeries>();
        DataTypeVariant const input{source};
        auto const result = createTemporalSubset(
                input,
                TimeFrameInterval{TimeFrameIndex{10}, TimeFrameIndex{5}},
                error);
        REQUIRE_FALSE(result.has_value());
        REQUIRE_FALSE(error.empty());
    }

    SECTION("TensorData unsupported in v1") {
        auto source = std::make_shared<TensorData>();
        DataTypeVariant const input{source};
        auto const result = createTemporalSubset(
                input,
                TimeFrameIndex{0},
                error);
        REQUIRE_FALSE(result.has_value());
        REQUIRE_FALSE(error.empty());
    }

    SECTION("DataManager missing key") {
        auto dm = makeDataManager();
        auto const result = createTemporalSubset(
                *dm,
                "missing_key",
                TimeFrameInterval{TimeFrameIndex{0}, TimeFrameIndex{0}},
                error);
        REQUIRE_FALSE(result.has_value());
        REQUIRE_FALSE(error.empty());
    }
}

TEST_CASE("createTemporalSubset - DataManager key overload", "[TemporalSubset][DataManager]") {
    auto dm = makeDataManager();
    auto source = std::make_shared<AnalogTimeSeries>(
            std::vector<float>{1.0f, 2.0f},
            std::vector<TimeFrameIndex>{TimeFrameIndex{0}, TimeFrameIndex{10}});
    dm->setData<AnalogTimeSeries>("analog", source, TimeKey{"time"});

    std::string error;
    auto const result = createTemporalSubset(
            *dm,
            "analog",
            TimeFrameInterval{TimeFrameIndex{0}, TimeFrameIndex{0}},
            error);

    auto const view = requireSubsetPtr<AnalogTimeSeries>(result);
    REQUIRE(error.empty());
    REQUIRE(view->getNumSamples() == 1);
}
