#include "LineExtend.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/builders/constants.hpp"
#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/line/clip_scenarios.hpp"

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;
using namespace pipeline_json_test;

static Line2D getLineAt(LineData const * line_data, TimeFrameIndex time) {
    auto const & lines = line_data->getAtTime(time);
    if (lines.empty()) {
        return Line2D{};
    }
    return lines[0];
}

TEST_CASE("V2 Binary Element Transform: LineExtend - Core Functionality",
          "[transforms][v2][binary_element][line_extend]") {

    LineExtendParams params;
    params.extend_end = ExtendEndpoint::Distal;
    params.tangent_distance_pixels = 20.0f;

    SECTION("Distal extension reaches reference beyond line end") {
        auto line_data = line_clip_scenarios::horizontal_line();
        auto reference_data = line_clip_scenarios::vertical_reference_no_intersection();

        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));
        auto reference_line = getLineAt(reference_data.get(), TimeFrameIndex(0));

        auto extended = extendLineAtReference(line, reference_line, params);

        REQUIRE(extended.size() == line.size() + 1);
        REQUIRE(extended.back().x == Catch::Approx(5.0f).margin(0.001f));
        REQUIRE(extended.back().y == Catch::Approx(2.0f).margin(0.001f));
    }

    SECTION("No forward intersection leaves line unchanged") {
        auto line_data = line_clip_scenarios::horizontal_line();
        auto reference_data = line_clip_scenarios::vertical_reference_at_2_5();

        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));
        auto reference_line = getLineAt(reference_data.get(), TimeFrameIndex(0));

        auto extended = extendLineAtReference(line, reference_line, params);

        REQUIRE(extended.size() == line.size());
        REQUIRE(extended.back().x == Catch::Approx(4.0f).margin(0.001f));
    }
}

TEST_CASE("V2 Binary Element Transform: LineExtend - Registry Integration",
          "[transforms][v2][binary_element][line_extend][registry]") {

    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered") {
        auto names = registry.getAllTransformNames();
        bool found = false;
        for (auto const & name: names) {
            if (name == "ExtendLineAtReference") {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("Metadata is available") {
        auto const * meta = registry.getMetadata("ExtendLineAtReference");
        REQUIRE(meta != nullptr);
        REQUIRE(meta->name == "ExtendLineAtReference");
        REQUIRE(meta->category == "Geometry");
        REQUIRE(meta->is_multi_input == true);
        REQUIRE(meta->input_arity == 2);
    }
}

TEST_CASE("V2 LineExtendParams - JSON Rejection",
          "[transforms][v2][params][json][line_extend]") {

    SECTION("Reject unknown extend_end") {
        std::string const json = R"({"extend_end": "invalid"})";
        REQUIRE_FALSE(loadParametersFromJson<LineExtendParams>(json));
    }

    SECTION("Reject non-string extend_end") {
        std::string const json = R"({"extend_end": 1})";
        REQUIRE_FALSE(loadParametersFromJson<LineExtendParams>(json));
    }
}

TEST_CASE("V2 DataManager Integration: LineExtend via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_extend]") {

    DataManager dm;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    ImageSize const image_size{
            static_cast<int>(test_fixture_constants::DEFAULT_IMAGE_WIDTH),
            static_cast<int>(test_fixture_constants::DEFAULT_IMAGE_HEIGHT)};

    auto line_data = std::make_shared<LineData>();
    line_data->setTimeFrame(time_frame);
    Line2D horizontal_line;
    horizontal_line.push_back({0.0f, 2.0f});
    horizontal_line.push_back({1.0f, 2.0f});
    horizontal_line.push_back({2.0f, 2.0f});
    horizontal_line.push_back({3.0f, 2.0f});
    horizontal_line.push_back({4.0f, 2.0f});
    line_data->addAtTime(TimeFrameIndex(100), horizontal_line, NotifyObservers::No);
    line_data->setImageSize(image_size);
    dm.setData("line_to_extend", line_data, TimeKey("default"));

    auto reference_data = std::make_shared<LineData>();
    reference_data->setTimeFrame(time_frame);
    Line2D vertical_ref;
    vertical_ref.push_back({5.0f, 0.0f});
    vertical_ref.push_back({5.0f, 5.0f});
    reference_data->addAtTime(TimeFrameIndex(100), vertical_ref, NotifyObservers::No);
    reference_data->setImageSize(image_size);
    dm.setData("reference_line", reference_data, TimeKey("default"));

    LineExtendParams params;
    params.extend_end = ExtendEndpoint::Distal;

    auto const pipeline = makeSingleStepPipeline(
            "ExtendLineAtReference",
            "line_to_extend",
            "v2_extended_lines",
            params,
            "1",
            std::vector<std::string>{"reference_line"});

    executeViaLoadDataFromJsonConfigV2(dm, pipeline);

    auto result_lines = dm.getData<LineData>("v2_extended_lines");
    REQUIRE(result_lines != nullptr);

    auto extended = result_lines->getAtTime(TimeFrameIndex(100));
    REQUIRE(extended.size() == 1);
    REQUIRE(extended[0].back().x == Catch::Approx(5.0f).margin(0.001f));
    REQUIRE(extended[0].back().y == Catch::Approx(2.0f).margin(0.001f));
}
