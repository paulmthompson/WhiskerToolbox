#include "transforms/Masks/Mask_Principal_Axis/mask_principal_axis.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <memory>

#include "fixtures/scenarios/mask/principal_axis_scenarios.hpp"

// ============================================================================
// Core Functionality Tests (using scenarios)
// ============================================================================

TEST_CASE("Mask principal axis - Empty mask data",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::empty_mask_data();
    auto result = calculate_mask_principal_axis(mask_data.get());

    REQUIRE(result->getTimesWithData().empty());
}

TEST_CASE("Mask principal axis - Single point mask (insufficient)",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::single_point_mask_principal_axis();
    auto result = calculate_mask_principal_axis(mask_data.get());

    // Should be empty since single point masks are skipped
    REQUIRE(result->getTimesWithData().empty());
}

TEST_CASE("Mask principal axis - Horizontal line major axis",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::horizontal_line_mask();

    auto params = std::make_unique<MaskPrincipalAxisParameters>();
    params->axis_type = PrincipalAxisType::Major;

    auto result = calculate_mask_principal_axis(mask_data.get(), params.get());

    auto const & times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(20));

    auto const & lines = result->getAtTime(TimeFrameIndex(20));
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].size() == 2);// Line should have 2 points

    // For a horizontal line, major axis should be roughly horizontal
    auto const & line = lines[0];
    float dx = line[1].x - line[0].x;
    float dy = line[1].y - line[0].y;
    float angle = std::atan2(std::abs(dy), std::abs(dx));

    // Angle should be close to 0 or π (horizontal)
    REQUIRE((angle < 0.2f || angle > (M_PI - 0.2f)));// Within ~11 degrees of horizontal
}

TEST_CASE("Mask principal axis - Vertical line major axis",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::vertical_line_mask();

    auto params = std::make_unique<MaskPrincipalAxisParameters>();
    params->axis_type = PrincipalAxisType::Major;

    auto result = calculate_mask_principal_axis(mask_data.get(), params.get());

    auto const & lines = result->getAtTime(TimeFrameIndex(30));
    REQUIRE(lines.size() == 1);

    // For a vertical line, major axis should be roughly vertical
    auto const & line = lines[0];
    float dx = line[1].x - line[0].x;
    float dy = line[1].y - line[0].y;
    float angle = std::atan2(std::abs(dy), std::abs(dx));

    // Angle should be close to π/2 (vertical)
    REQUIRE_THAT(angle, Catch::Matchers::WithinAbs(M_PI / 2.0f, 0.2f));// Within ~11 degrees of vertical
}

TEST_CASE("Mask principal axis - Diagonal line major axis",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::diagonal_line_mask();

    auto params = std::make_unique<MaskPrincipalAxisParameters>();
    params->axis_type = PrincipalAxisType::Major;

    auto result = calculate_mask_principal_axis(mask_data.get(), params.get());

    auto const & lines = result->getAtTime(TimeFrameIndex(40));
    REQUIRE(lines.size() == 1);

    // For a 45-degree line, major axis should be roughly at 45 degrees
    auto const & line = lines[0];
    float dx = line[1].x - line[0].x;
    float dy = line[1].y - line[0].y;
    float angle = std::atan2(dy, dx);

    // Angle should be close to π/4 or -3π/4 (45 degrees or equivalent)
    float normalized_angle = std::fmod(std::abs(angle), static_cast<float>(M_PI) / 2.0f);
    REQUIRE_THAT(normalized_angle, Catch::Matchers::WithinAbs(M_PI / 4.0f, 0.2f));
}

TEST_CASE("Mask principal axis - Rectangle major vs minor axis",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::wide_rectangle_mask();

    // Test major axis
    auto major_params = std::make_unique<MaskPrincipalAxisParameters>();
    major_params->axis_type = PrincipalAxisType::Major;
    auto major_result = calculate_mask_principal_axis(mask_data.get(), major_params.get());

    // Test minor axis
    auto minor_params = std::make_unique<MaskPrincipalAxisParameters>();
    minor_params->axis_type = PrincipalAxisType::Minor;
    auto minor_result = calculate_mask_principal_axis(mask_data.get(), minor_params.get());

    auto const & major_lines = major_result->getAtTime(TimeFrameIndex(50));
    auto const & minor_lines = minor_result->getAtTime(TimeFrameIndex(50));

    REQUIRE(major_lines.size() == 1);
    REQUIRE(minor_lines.size() == 1);

    // Major axis should be more horizontal (rectangle is wider than tall)
    auto const & major_line = major_lines[0];
    float major_dx = major_line[1].x - major_line[0].x;
    float major_dy = major_line[1].y - major_line[0].y;
    float major_angle = std::atan2(std::abs(major_dy), std::abs(major_dx));

    // Minor axis should be more vertical
    auto const & minor_line = minor_lines[0];
    float minor_dx = minor_line[1].x - minor_line[0].x;
    float minor_dy = minor_line[1].y - minor_line[0].y;
    float minor_angle = std::atan2(std::abs(minor_dy), std::abs(minor_dx));

    // Major axis should be more horizontal than vertical
    REQUIRE(major_angle < M_PI / 4.0f);
    // Minor axis should be more vertical than horizontal
    REQUIRE(minor_angle > M_PI / 4.0f);
}

TEST_CASE("Mask principal axis - Multiple masks at one timestamp",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::multiple_masks_principal_axis();

    auto result = calculate_mask_principal_axis(mask_data.get());

    auto const & lines = result->getAtTime(TimeFrameIndex(60));
    REQUIRE(lines.size() == 2);// Two masks = two principal axis lines
}

TEST_CASE("Mask principal axis - Image size is preserved",
          "[mask][principal_axis][transform][scenario]") {
    auto mask_data = mask_scenarios::mask_with_image_size_principal_axis();

    auto result = calculate_mask_principal_axis(mask_data.get());

    // Verify image size is copied
    REQUIRE(result->getImageSize().width == 640);
    REQUIRE(result->getImageSize().height == 480);
}

// ============================================================================
// Edge Cases (using scenarios)
// ============================================================================

TEST_CASE("Mask principal axis - Two identical points",
          "[mask][principal_axis][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::identical_points_mask();

    auto result = calculate_mask_principal_axis(mask_data.get());

    // Should still produce a result, even if axis direction is arbitrary
    auto const & times = result->getTimesWithData();
    if (!times.empty()) {
        auto const & lines = result->getAtTime(TimeFrameIndex(10));
        REQUIRE(lines.size() <= 1);// Should produce at most one line
    }
}

TEST_CASE("Mask principal axis - Circular point distribution",
          "[mask][principal_axis][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::circular_mask();

    auto result = calculate_mask_principal_axis(mask_data.get());

    auto const & lines = result->getAtTime(TimeFrameIndex(20));
    REQUIRE(lines.size() == 1);

    // For a circle, major and minor axes should be similar in magnitude
    // (though direction may be arbitrary due to symmetry)
    REQUIRE(lines[0].size() == 2);
}

TEST_CASE("Mask principal axis - Large coordinates",
          "[mask][principal_axis][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::large_coordinates_principal_axis();

    auto result = calculate_mask_principal_axis(mask_data.get());

    auto const & lines = result->getAtTime(TimeFrameIndex(30));
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].size() == 2);
}

TEST_CASE("Mask principal axis - Null input handling",
          "[mask][principal_axis][transform][edge]") {
    auto result = calculate_mask_principal_axis(nullptr);
    REQUIRE(result->getTimesWithData().empty());
}

// ============================================================================
// Operation Interface Tests
// ============================================================================

TEST_CASE("MaskPrincipalAxisOperation - Operation metadata",
          "[mask][principal_axis][operation]") {
    MaskPrincipalAxisOperation operation;

    REQUIRE(operation.getName() == "Calculate Mask Principal Axis");
    REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));
    
    auto params = operation.getDefaultParameters();
    REQUIRE(params != nullptr);

    auto * specific_params = dynamic_cast<MaskPrincipalAxisParameters *>(params.get());
    REQUIRE(specific_params != nullptr);
    REQUIRE(specific_params->axis_type == PrincipalAxisType::Major);// Default should be major
}

TEST_CASE("MaskPrincipalAxisOperation - Can apply to valid MaskData",
          "[mask][principal_axis][operation][scenario]") {
    MaskPrincipalAxisOperation operation;
    auto mask_data = mask_scenarios::horizontal_line_mask();
    DataTypeVariant variant = mask_data;
    REQUIRE(operation.canApply(variant));
}

TEST_CASE("MaskPrincipalAxisOperation - Cannot apply to null MaskData",
          "[mask][principal_axis][operation]") {
    MaskPrincipalAxisOperation operation;
    std::shared_ptr<MaskData> null_mask;
    DataTypeVariant variant = null_mask;
    REQUIRE_FALSE(operation.canApply(variant));
}

TEST_CASE("MaskPrincipalAxisOperation - Execute with major axis",
          "[mask][principal_axis][operation][scenario]") {
    MaskPrincipalAxisOperation operation;
    auto mask_data = mask_scenarios::operation_execute_horizontal();

    DataTypeVariant input_variant = mask_data;

    auto params = std::make_unique<MaskPrincipalAxisParameters>();
    params->axis_type = PrincipalAxisType::Major;

    auto result_variant = operation.execute(input_variant, params.get());

    REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));

    auto result = std::get<std::shared_ptr<LineData>>(result_variant);
    REQUIRE(result != nullptr);

    auto const & lines = result->getAtTime(TimeFrameIndex(50));
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].size() == 2);
}

TEST_CASE("MaskPrincipalAxisOperation - Execute with minor axis",
          "[mask][principal_axis][operation][scenario]") {
    MaskPrincipalAxisOperation operation;
    auto mask_data = mask_scenarios::operation_execute_vertical();

    DataTypeVariant input_variant = mask_data;

    auto params = std::make_unique<MaskPrincipalAxisParameters>();
    params->axis_type = PrincipalAxisType::Minor;

    auto result_variant = operation.execute(input_variant, params.get());

    REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));

    auto result = std::get<std::shared_ptr<LineData>>(result_variant);
    REQUIRE(result != nullptr);

    auto const & lines = result->getAtTime(TimeFrameIndex(60));
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].size() == 2);
}

// ============================================================================
// JSON Pipeline Tests
// ============================================================================

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Mask Principal Axis - JSON pipeline",
          "[mask][principal_axis][transform][json][scenario]") {
    // Create DataManager with time frame
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Add test data from scenario
    auto test_mask = mask_scenarios::json_pipeline_multi_line_principal_axis();
    test_mask->setTimeFrame(time_frame);
    dm.setData("test_mask", test_mask, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Principal Axis Pipeline\",\n"
        "            \"description\": \"Test mask principal axis calculation on mask data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Mask Principal Axis\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"principal_axes\",\n"
        "                \"parameters\": {\n"
        "                    \"axis_type\": \"Major\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_principal_axis_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    // Execute the transformation pipeline using load_data_from_json_config
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_lines = dm.getData<LineData>("principal_axes");
    REQUIRE(result_lines != nullptr);
    
    // Verify the principal axis results - should have lines at all three timestamps
    auto const & times = result_lines->getTimesWithData();
    REQUIRE(times.size() == 3);
    
    // Verify each timestamp has exactly one line with 2 points
    for (auto const & time : times) {
        auto const & lines = result_lines->getAtTime(time);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 2);
    }
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("Data Transform: Mask Principal Axis - Minor axis JSON pipeline",
          "[mask][principal_axis][transform][json][scenario]") {
    // Create DataManager with time frame
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Add test data from scenario
    auto test_mask = mask_scenarios::json_pipeline_multi_line_principal_axis();
    test_mask->setTimeFrame(time_frame);
    dm.setData("test_mask", test_mask, TimeKey("default"));
    
    const char* json_config_minor = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Principal Axis Minor Pipeline\",\n"
        "            \"description\": \"Test mask principal axis calculation with minor axis\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Mask Principal Axis\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"minor_axes\",\n"
        "                \"parameters\": {\n"
        "                    \"axis_type\": \"Minor\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_principal_axis_minor_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_minor = test_dir / "pipeline_config_minor.json";
    {
        std::ofstream json_file(json_filepath_minor);
        REQUIRE(json_file.is_open());
        json_file << json_config_minor;
        json_file.close();
    }
    
    // Execute the minor axis pipeline
    auto data_info_list_minor = load_data_from_json_config(&dm, json_filepath_minor.string());
    
    // Verify the minor axis results
    auto result_lines_minor = dm.getData<LineData>("minor_axes");
    REQUIRE(result_lines_minor != nullptr);
    
    auto const & times_minor = result_lines_minor->getTimesWithData();
    REQUIRE(times_minor.size() == 3);
    
    // Verify each timestamp has exactly one line with 2 points
    for (auto const & time : times_minor) {
        auto const & lines = result_lines_minor->getAtTime(time);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 2);
    }
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("Data Transform: Mask Principal Axis - Rectangle major axis JSON pipeline",
          "[mask][principal_axis][transform][json][scenario]") {
    // Create DataManager with time frame
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Add test data from scenario
    auto rectangle_mask = mask_scenarios::json_pipeline_rectangle_principal_axis();
    rectangle_mask->setTimeFrame(time_frame);
    dm.setData("rectangle_mask", rectangle_mask, TimeKey("default"));
    
    const char* json_config_rect_major = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Rectangle Major Axis Pipeline\",\n"
        "            \"description\": \"Test major axis on rectangle mask\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Mask Principal Axis\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"rectangle_mask\",\n"
        "                \"output_key\": \"rectangle_major_axes\",\n"
        "                \"parameters\": {\n"
        "                    \"axis_type\": \"Major\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_principal_axis_rect_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_rect_major = test_dir / "pipeline_config_rect_major.json";
    {
        std::ofstream json_file(json_filepath_rect_major);
        REQUIRE(json_file.is_open());
        json_file << json_config_rect_major;
        json_file.close();
    }
    
    // Execute the rectangle major axis pipeline
    auto data_info_list_rect_major = load_data_from_json_config(&dm, json_filepath_rect_major.string());
    
    // Verify the rectangle major axis results
    auto result_lines_rect_major = dm.getData<LineData>("rectangle_major_axes");
    REQUIRE(result_lines_rect_major != nullptr);
    
    auto const & times_rect = result_lines_rect_major->getTimesWithData();
    REQUIRE(times_rect.size() == 1);
    
    auto const & lines_rect = result_lines_rect_major->getAtTime(TimeFrameIndex(400));
    REQUIRE(lines_rect.size() == 1);
    REQUIRE(lines_rect[0].size() == 2);
    
    // Verify major axis is more horizontal than vertical for the rectangle
    auto const & line = lines_rect[0];
    float dx = line[1].x - line[0].x;
    float dy = line[1].y - line[0].y;
    float angle = std::atan2(std::abs(dy), std::abs(dx));
    
    // Major axis should be more horizontal than vertical for a wide rectangle
    REQUIRE(angle < M_PI / 4.0f);
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}