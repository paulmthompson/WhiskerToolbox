#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "Lines/Line_Data.hpp"
#include "transforms/Lines/Line_Extend/line_extend.hpp"
#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

#include "fixtures/scenarios/line/clip_scenarios.hpp"

TEST_CASE("Data Transform: Extend Line by Reference Line - Happy Path", "[transforms][line_extend]") {
    LineExtendParameters params;

    SECTION("Distal extension reaches reference beyond line end") {
        auto line_data = line_clip_scenarios::horizontal_line();
        auto reference_line_data = line_clip_scenarios::vertical_reference_no_intersection();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.extend_end = ExtendEndpoint::Distal;
        params.tangent_distance_pixels = 20.0f;

        auto result_lines = extend_lines(line_data.get(), &params);

        auto const & extended_lines = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(extended_lines.size() == 1);
        REQUIRE(extended_lines[0].size() == 6);
        REQUIRE(extended_lines[0].back().x == Catch::Approx(5.0f).margin(0.001f));
        REQUIRE(extended_lines[0].back().y == Catch::Approx(2.0f).margin(0.001f));
    }

    SECTION("No forward intersection leaves line unchanged") {
        auto line_data = line_clip_scenarios::horizontal_line();
        auto reference_line_data = line_clip_scenarios::vertical_reference_at_2_5();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.extend_end = ExtendEndpoint::Distal;

        auto result_lines = extend_lines(line_data.get(), &params);

        auto const & extended_lines = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(extended_lines.size() == 1);
        REQUIRE(extended_lines[0].size() == 5);
        REQUIRE(extended_lines[0].back().x == Catch::Approx(4.0f).margin(0.001f));
    }
}

TEST_CASE("Data Transform: Extend Line by Reference Line - Parameter Factory", "[transforms][line_extend][factory]") {
    auto & factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LineExtendParameters>();
    REQUIRE(params_base != nullptr);

    nlohmann::json const params_json = {
            {"reference_frame", 2},
            {"extend_end", "Base"},
            {"tangent_distance_pixels", 15.5},
    };

    for (auto const & [key, val]: params_json.items()) {
        factory.setParameter("Extend Line by Reference Line", params_base.get(), key, val, nullptr);
    }

    auto * params = dynamic_cast<LineExtendParameters *>(params_base.get());
    REQUIRE(params != nullptr);
    REQUIRE(params->reference_frame == 2);
    REQUIRE(params->extend_end == ExtendEndpoint::Base);
    REQUIRE(params->tangent_distance_pixels == Catch::Approx(15.5f));
}

TEST_CASE("Data Transform: Extend Line by Reference Line - Registry", "[transforms][line_extend][registry]") {
    TransformRegistry registry;
    auto const * operation = registry.findOperationByName("Extend Line by Reference Line");
    REQUIRE(operation != nullptr);
    REQUIRE(operation->getName() == "Extend Line by Reference Line");
}
