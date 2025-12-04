#include "Lineage/LineageRecorder.hpp"
#include "Lineage/LineageRegistry.hpp"
#include "Lineage/LineageTypes.hpp"
#include "transforms/v2/core/TransformTypes.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace WhiskerToolbox::Lineage;
using namespace WhiskerToolbox::Transforms::V2;

TEST_CASE("LineageRecorder - Basic Recording", "[lineage][recorder]") {
    LineageRegistry registry;

    SECTION("Record OneToOneByTime lineage") {
        LineageRecorder::record(registry, "mask_areas", "masks", TransformLineageType::OneToOneByTime);

        REQUIRE(registry.hasLineage("mask_areas"));
        auto desc = registry.getLineage("mask_areas");
        REQUIRE(desc.has_value());

        // Should be OneToOneByTime variant
        auto const * one_to_one = std::get_if<OneToOneByTime>(&desc.value());
        REQUIRE(one_to_one != nullptr);
        CHECK(one_to_one->source_key == "masks");
    }

    SECTION("Record AllToOneByTime lineage") {
        LineageRecorder::record(registry, "total_area", "areas", TransformLineageType::AllToOneByTime);

        REQUIRE(registry.hasLineage("total_area"));
        auto desc = registry.getLineage("total_area");
        REQUIRE(desc.has_value());

        auto const * all_to_one = std::get_if<AllToOneByTime>(&desc.value());
        REQUIRE(all_to_one != nullptr);
        CHECK(all_to_one->source_key == "areas");
    }

    SECTION("Record Source lineage") {
        LineageRecorder::record(registry, "original_data", "", TransformLineageType::Source);

        REQUIRE(registry.hasLineage("original_data"));
        REQUIRE(registry.isSource("original_data"));
    }

    SECTION("None lineage does not record") {
        LineageRecorder::record(registry, "temp_data", "input", TransformLineageType::None);

        CHECK_FALSE(registry.hasLineage("temp_data"));
    }

    SECTION("Subset lineage throws (requires explicit entity list)") {
        CHECK_THROWS_AS(
                LineageRecorder::record(registry, "filtered", "input", TransformLineageType::Subset),
                std::invalid_argument);
    }
}

TEST_CASE("LineageRecorder - recordSource", "[lineage][recorder]") {
    LineageRegistry registry;

    LineageRecorder::recordSource(registry, "loaded_masks");

    REQUIRE(registry.hasLineage("loaded_masks"));
    REQUIRE(registry.isSource("loaded_masks"));

    auto desc = registry.getLineage("loaded_masks");
    REQUIRE(desc.has_value());
    CHECK(std::holds_alternative<Source>(desc.value()));
}

TEST_CASE("LineageRecorder - Multi-input Recording", "[lineage][recorder]") {
    LineageRegistry registry;

    SECTION("Record multi-input OneToOneByTime") {
        std::vector<std::string> inputs = {"lines", "points"};
        LineageRecorder::recordMultiInput(registry, "distances", inputs, TransformLineageType::OneToOneByTime);

        REQUIRE(registry.hasLineage("distances"));
        auto desc = registry.getLineage("distances");
        REQUIRE(desc.has_value());

        auto const * multi = std::get_if<MultiSourceLineage>(&desc.value());
        REQUIRE(multi != nullptr);
        CHECK(multi->source_keys.size() == 2);
        CHECK(multi->source_keys[0] == "lines");
        CHECK(multi->source_keys[1] == "points");
        CHECK(multi->strategy == MultiSourceLineage::CombineStrategy::ZipByTime);
    }

    SECTION("Record multi-input AllToOneByTime") {
        std::vector<std::string> inputs = {"data_a", "data_b", "data_c"};
        LineageRecorder::recordMultiInput(registry, "combined", inputs, TransformLineageType::AllToOneByTime);

        REQUIRE(registry.hasLineage("combined"));
        auto desc = registry.getLineage("combined");
        auto const * multi = std::get_if<MultiSourceLineage>(&desc.value());
        REQUIRE(multi != nullptr);
        CHECK(multi->source_keys.size() == 3);
    }

    SECTION("None lineage does not record for multi-input") {
        std::vector<std::string> inputs = {"a", "b"};
        LineageRecorder::recordMultiInput(registry, "temp", inputs, TransformLineageType::None);
        CHECK_FALSE(registry.hasLineage("temp"));
    }

    SECTION("Empty input keys throws") {
        std::vector<std::string> empty_inputs;
        CHECK_THROWS_AS(
                LineageRecorder::recordMultiInput(registry, "out", empty_inputs, TransformLineageType::OneToOneByTime),
                std::invalid_argument);
    }

    SECTION("Source lineage for multi-input throws") {
        std::vector<std::string> inputs = {"a", "b"};
        CHECK_THROWS_AS(
                LineageRecorder::recordMultiInput(registry, "out", inputs, TransformLineageType::Source),
                std::invalid_argument);
    }

    SECTION("Subset lineage for multi-input throws") {
        std::vector<std::string> inputs = {"a", "b"};
        CHECK_THROWS_AS(
                LineageRecorder::recordMultiInput(registry, "out", inputs, TransformLineageType::Subset),
                std::invalid_argument);
    }
}

TEST_CASE("LineageRecorder - Overwriting lineage", "[lineage][recorder]") {
    LineageRegistry registry;

    // First record OneToOneByTime
    LineageRecorder::record(registry, "data", "source_a", TransformLineageType::OneToOneByTime);

    auto desc1 = registry.getLineage("data");
    REQUIRE(std::get_if<OneToOneByTime>(&desc1.value())->source_key == "source_a");

    // Overwrite with AllToOneByTime
    LineageRecorder::record(registry, "data", "source_b", TransformLineageType::AllToOneByTime);

    auto desc2 = registry.getLineage("data");
    auto const * all_to_one = std::get_if<AllToOneByTime>(&desc2.value());
    REQUIRE(all_to_one != nullptr);
    CHECK(all_to_one->source_key == "source_b");
}
