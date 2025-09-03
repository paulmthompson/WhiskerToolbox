#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "Lines/Line_Data.hpp"
#include "transforms/Lines/Line_Resample/line_resample.hpp"
#include "transforms/data_transforms.hpp"// For ProgressCallback

#include <functional>// std::function
#include <memory>    // std::make_shared
#include <vector>

// Using Catch::Matchers::Equals for vectors of floats.

TEST_CASE("Data Transform: Line Resample - Happy Path", "[transforms][line_resample]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<LineData> result_data;
    LineResampleParameters params;
    int volatile progress_val = -1;// Volatile to prevent optimization issues in test
    int volatile call_count = 0;   // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("FixedSpacing algorithm") {
        // Create test line data with multiple lines at different times
        line_data = std::make_shared<LineData>();
        line_data->setImageSize(ImageSize(1000, 1000));

        // Add a simple line at time 100
        std::vector<float> x1 = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<float> y1 = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        line_data->addAtTime(TimeFrameIndex(100), x1, y1);

        // Add another line at time 200
        std::vector<float> x2 = {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f};
        std::vector<float> y2 = {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f};
        line_data->addAtTime(TimeFrameIndex(200), x2, y2);

        params.algorithm = LineSimplificationAlgorithm::FixedSpacing;
        params.target_spacing = 15.0f;
        params.epsilon = 2.0f;

        result_data = line_resample(line_data.get(), params);
        REQUIRE(result_data != nullptr);
        REQUIRE(result_data->getImageSize() == line_data->getImageSize());

        // Check that we have data at both times
        auto times_with_data = result_data->getTimesWithData();
        std::vector<TimeFrameIndex> expected_times = {TimeFrameIndex(100), TimeFrameIndex(200)};
        std::vector<TimeFrameIndex> actual_times(times_with_data.begin(), times_with_data.end());
        REQUIRE_THAT(actual_times, Catch::Matchers::Equals(expected_times));

        progress_val = -1;
        call_count = 0;
        result_data = line_resample(line_data.get(), params, cb);
        REQUIRE(result_data != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("DouglasPeucker algorithm") {
        // Create test line data with a complex line
        line_data = std::make_shared<LineData>();
        line_data->setImageSize(ImageSize(1000, 1000));

        // Add a line with many points that can be simplified
        std::vector<float> x = {10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 20.0f};
        std::vector<float> y = {10.0f, 10.1f, 10.2f, 10.3f, 10.4f, 10.5f, 10.6f, 10.7f, 10.8f, 10.9f, 11.0f};
        line_data->addAtTime(TimeFrameIndex(100), x, y);

        params.algorithm = LineSimplificationAlgorithm::DouglasPeucker;
        params.target_spacing = 5.0f;
        params.epsilon = 0.5f;

        result_data = line_resample(line_data.get(), params);
        REQUIRE(result_data != nullptr);
        REQUIRE(result_data->getImageSize() == line_data->getImageSize());

        // Check that we have data at the time
        auto times_with_data = result_data->getTimesWithData();
        std::vector<TimeFrameIndex> expected_times = {TimeFrameIndex(100)};
        std::vector<TimeFrameIndex> actual_times(times_with_data.begin(), times_with_data.end());
        REQUIRE_THAT(actual_times, Catch::Matchers::Equals(expected_times));

        // The simplified line should have fewer points than the original
        auto original_lines = line_data->getAtTime(TimeFrameIndex(100));
        auto result_lines = result_data->getAtTime(TimeFrameIndex(100));
        REQUIRE(original_lines.size() == result_lines.size());     // Same number of lines
        REQUIRE(original_lines[0].size() > result_lines[0].size());// Fewer points in simplified line
    }

    SECTION("Empty line data") {
        line_data = std::make_shared<LineData>();
        line_data->setImageSize(ImageSize(1000, 1000));

        params.algorithm = LineSimplificationAlgorithm::FixedSpacing;
        params.target_spacing = 10.0f;
        params.epsilon = 2.0f;

        result_data = line_resample(line_data.get(), params);
        REQUIRE(result_data != nullptr);
        REQUIRE(result_data->getImageSize() == line_data->getImageSize());
        REQUIRE(result_data->getTimesWithData().empty());
    }

    SECTION("Line with empty lines") {
        line_data = std::make_shared<LineData>();
        line_data->setImageSize(ImageSize(1000, 1000));

        // Add a normal line
        std::vector<float> x1 = {10.0f, 20.0f, 30.0f};
        std::vector<float> y1 = {10.0f, 20.0f, 30.0f};
        line_data->addAtTime(TimeFrameIndex(100), x1, y1);

        // Add an empty line
        std::vector<float> x2 = {};
        std::vector<float> y2 = {};
        line_data->addAtTime(TimeFrameIndex(200), x2, y2);

        params.algorithm = LineSimplificationAlgorithm::FixedSpacing;
        params.target_spacing = 10.0f;
        params.epsilon = 2.0f;

        result_data = line_resample(line_data.get(), params);
        REQUIRE(result_data != nullptr);
        REQUIRE(result_data->getImageSize() == line_data->getImageSize());

        // Should have data at both times
        auto times_with_data = result_data->getTimesWithData();
        std::vector<TimeFrameIndex> expected_times = {TimeFrameIndex(100), TimeFrameIndex(200)};
        std::vector<TimeFrameIndex> actual_times(times_with_data.begin(), times_with_data.end());
        REQUIRE_THAT(actual_times, Catch::Matchers::Equals(expected_times));

        // Time 100 should have a non-empty line, time 200 should have an empty line
        auto lines_at_100 = result_data->getAtTime(TimeFrameIndex(100));
        auto lines_at_200 = result_data->getAtTime(TimeFrameIndex(200));
        REQUIRE(lines_at_100.size() == 1);
        REQUIRE(!lines_at_100[0].empty());
        REQUIRE(lines_at_200.size() == 1);
        REQUIRE(lines_at_200[0].empty());
    }
}

TEST_CASE("Data Transform: Line Resample - Error and Edge Cases", "[transforms][line_resample]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<LineData> result_data;
    LineResampleParameters params;
    int volatile progress_val = -1;
    int volatile call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Single point line") {
        line_data = std::make_shared<LineData>();
        line_data->setImageSize(ImageSize(1000, 1000));

        std::vector<float> x = {10.0f};
        std::vector<float> y = {10.0f};
        line_data->addAtTime(TimeFrameIndex(100), x, y);

        params.algorithm = LineSimplificationAlgorithm::FixedSpacing;
        params.target_spacing = 10.0f;
        params.epsilon = 2.0f;

        result_data = line_resample(line_data.get(), params);
        REQUIRE(result_data != nullptr);
        REQUIRE(result_data->getImageSize() == line_data->getImageSize());

        auto lines = result_data->getAtTime(TimeFrameIndex(100));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 1);// Should still have one point
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Line Resample - JSON pipeline", "[transforms][line_resample][json]") {
    nlohmann::json const json_config = {
            {"steps", {{{"step_id", "resample_step_1"}, {"transform_name", "Resample Line"}, {"input_key", "TestLines.channel1"}, {"output_key", "ResampledLines"}, {"parameters", {{"algorithm", "FixedSpacing"}, {"target_spacing", 15.0}, {"epsilon", 2.0}}}}}}};

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data
    auto line_data = std::make_shared<LineData>();
    line_data->setImageSize(ImageSize(1000, 1000));

    // Add test lines at different times
    std::vector<float> x1 = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    std::vector<float> y1 = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    line_data->addAtTime(TimeFrameIndex(100), x1, y1);

    std::vector<float> x2 = {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f};
    std::vector<float> y2 = {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f};
    line_data->addAtTime(TimeFrameIndex(200), x2, y2);

    line_data->setTimeFrame(time_frame);
    dm.setData("TestLines.channel1", line_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto result_line_data = dm.getData<LineData>("ResampledLines");
    REQUIRE(result_line_data != nullptr);
    REQUIRE(result_line_data->getImageSize() == line_data->getImageSize());

    // Check that we have data at both times
    auto times_with_data = result_line_data->getTimesWithData();
    std::vector<TimeFrameIndex> expected_times = {TimeFrameIndex(100), TimeFrameIndex(200)};
    std::vector<TimeFrameIndex> actual_times(times_with_data.begin(), times_with_data.end());
    REQUIRE_THAT(actual_times, Catch::Matchers::Equals(expected_times));
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Line Resample - Parameter Factory", "[transforms][line_resample][factory]") {
    auto & factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LineResampleParameters>();
    REQUIRE(params_base != nullptr);

    nlohmann::json const params_json = {
            {"algorithm", "Douglas-Peucker"},
            {"target_spacing", 25.0},
            {"epsilon", 5.0}};

    for (auto const & [key, val]: params_json.items()) {
        factory.setParameter("Resample Line", params_base.get(), key, val, nullptr);
    }

    auto * params = dynamic_cast<LineResampleParameters *>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->algorithm == LineSimplificationAlgorithm::DouglasPeucker);
    REQUIRE(params->target_spacing == 25.0f);
    REQUIRE(params->epsilon == 5.0f);
}

TEST_CASE("Data Transform: Line Resample - load_data_from_json_config", "[transforms][line_resample][json_config]") {
    // Create DataManager and populate it with LineData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data in code
    auto test_lines = std::make_shared<LineData>();
    test_lines->setImageSize(ImageSize(1000, 1000));

    // Add test lines at different times
    std::vector<float> x1 = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    std::vector<float> y1 = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    test_lines->addAtTime(TimeFrameIndex(100), x1, y1);

    std::vector<float> x2 = {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f};
    std::vector<float> y2 = {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f};
    test_lines->addAtTime(TimeFrameIndex(200), x2, y2);

    test_lines->setTimeFrame(time_frame);

    // Store the line data in DataManager with a known key
    dm.setData("test_lines", test_lines, TimeKey("default"));

    // Create JSON configuration for transformation pipeline using unified format
    char const * json_config =
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"Line Resample Pipeline\",\n"
            "            \"description\": \"Test line resampling on line data\",\n"
            "            \"version\": \"1.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"Resample Line\",\n"
            "                \"phase\": \"analysis\",\n"
            "                \"input_key\": \"test_lines\",\n"
            "                \"output_key\": \"resampled_lines\",\n"
            "                \"parameters\": {\n"
            "                    \"algorithm\": \"FixedSpacing\",\n"
            "                    \"target_spacing\": 15.0,\n"
            "                    \"epsilon\": 2.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";

    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_resample_pipeline_test";
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
    auto result_line_data = dm.getData<LineData>("resampled_lines");
    REQUIRE(result_line_data != nullptr);
    REQUIRE(result_line_data->getImageSize() == test_lines->getImageSize());

    // Verify the resampling results
    auto times_with_data = result_line_data->getTimesWithData();
    std::vector<TimeFrameIndex> expected_times = {TimeFrameIndex(100), TimeFrameIndex(200)};
    std::vector<TimeFrameIndex> actual_times(times_with_data.begin(), times_with_data.end());
    REQUIRE_THAT(actual_times, Catch::Matchers::Equals(expected_times));

    // Test another pipeline with different parameters (Douglas-Peucker)
    char const * json_config_dp =
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"Line Resample with Douglas-Peucker\",\n"
            "            \"description\": \"Test line resampling with Douglas-Peucker algorithm\",\n"
            "            \"version\": \"1.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"Resample Line\",\n"
            "                \"phase\": \"analysis\",\n"
            "                \"input_key\": \"test_lines\",\n"
            "                \"output_key\": \"resampled_lines_dp\",\n"
            "                \"parameters\": {\n"
            "                    \"algorithm\": \"Douglas-Peucker\",\n"
            "                    \"target_spacing\": 5.0,\n"
            "                    \"epsilon\": 3.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";

    std::filesystem::path json_filepath_dp = test_dir / "pipeline_config_dp.json";
    {
        std::ofstream json_file(json_filepath_dp);
        REQUIRE(json_file.is_open());
        json_file << json_config_dp;
        json_file.close();
    }

    // Execute the Douglas-Peucker pipeline
    auto data_info_list_dp = load_data_from_json_config(&dm, json_filepath_dp.string());

    // Verify the Douglas-Peucker results
    auto result_line_data_dp = dm.getData<LineData>("resampled_lines_dp");
    REQUIRE(result_line_data_dp != nullptr);
    REQUIRE(result_line_data_dp->getImageSize() == test_lines->getImageSize());

    // Check that we have data at both times
    auto times_with_data_dp = result_line_data_dp->getTimesWithData();
    std::vector<TimeFrameIndex> actual_times_dp(times_with_data_dp.begin(), times_with_data_dp.end());
    REQUIRE_THAT(actual_times_dp, Catch::Matchers::Equals(expected_times));

    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (std::exception const & e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
