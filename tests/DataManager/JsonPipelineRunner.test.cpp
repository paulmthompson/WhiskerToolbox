/**
 * @file JsonPipelineRunner.test.cpp
 * @brief Tests for the DataManager JSON pipeline runner.
 */

#include "JsonPipeline/JsonPipelineRunner.hpp"

#include "DataManager.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

/**
 * @brief RAII helper for a unique temporary pipeline test directory.
 */
class TempPipelineDirectory {
public:
    /**
     * @brief Create a unique temporary directory.
     * @post path() refers to an existing directory.
     */
    TempPipelineDirectory() {
        auto const stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path() /
                ("whiskertoolbox_json_pipeline_" + std::to_string(stamp));
        std::filesystem::create_directories(_path);
    }

    /**
     * @brief Remove the temporary directory and its contents.
     */
    ~TempPipelineDirectory() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    /**
     * @brief Get the temporary directory path.
     * @return Filesystem path for this test directory.
     * @post Returned path is the directory created by the constructor.
     */
    [[nodiscard]] std::filesystem::path const & path() const {
        return _path;
    }

private:
    std::filesystem::path _path;
};

/**
 * @brief Write a simple CSV point file used by the JSON loader.
 * @param filepath Destination CSV path.
 * @post The file contains two point rows with a header.
 */
void writeSimplePointCsv(std::filesystem::path const & filepath) {
    std::ofstream output(filepath);
    output << "frame,x,y\n";
    output << "0,1.5,2.5\n";
    output << "5,3.5,4.5\n";
}

}// namespace

TEST_CASE("JsonPipelineRunner loads object-root data config",
          "[DataManager][JsonPipelineRunner]") {
    TempPipelineDirectory temp_dir;
    auto const csv_path = temp_dir.path() / "points.csv";
    auto const json_path = temp_dir.path() / "pipeline.json";
    writeSimplePointCsv(csv_path);

    nlohmann::json const config = {
            {"variables", {{"root", temp_dir.path().string()}}},
            {"data",
             nlohmann::json::array({{{"data_type", "points"},
                                     {"name", "tracked_points"},
                                     {"filepath", "${root}/points.csv"},
                                     {"format", "csv"},
                                     {"csv_layout", "simple"},
                                     {"frame_column", 0},
                                     {"x_column", 1},
                                     {"y_column", 2},
                                     {"column_delim", ","}}})}};

    {
        std::ofstream output(json_path);
        output << config.dump(2);
    }

    DataManager data_manager;
    auto const result = WhiskerToolbox::DataManagerPipeline::runJsonPipelineFile(
            data_manager,
            json_path.string());

    REQUIRE(result.m_success);
    REQUIRE(result.m_failed_phase == WhiskerToolbox::DataManagerPipeline::JsonPipelinePhase::None);

    auto loaded = data_manager.getData<PointData>("tracked_points");
    REQUIRE(loaded != nullptr);

    auto points_at_zero = loaded->getAtTime(TimeFrameIndex(0));
    REQUIRE(points_at_zero.size() == 1);
    REQUIRE(points_at_zero.front().x == Catch::Approx(1.5f));
    REQUIRE(points_at_zero.front().y == Catch::Approx(2.5f));
}

TEST_CASE("JsonPipelineRunner executes root-level saves",
          "[DataManager][JsonPipelineRunner]") {
    TempPipelineDirectory temp_dir;
    auto const csv_path = temp_dir.path() / "points.csv";
    auto const json_path = temp_dir.path() / "pipeline_with_save.json";
    auto const save_path = temp_dir.path() / "saved_points.csv";
    writeSimplePointCsv(csv_path);

    nlohmann::json const config = {
            {"variables", {{"root", temp_dir.path().string()}, {"output_name", "saved_points.csv"}}},
            {"data",
             nlohmann::json::array({{{"data_type", "points"},
                                     {"name", "tracked_points"},
                                     {"filepath", "${root}/points.csv"},
                                     {"format", "csv"},
                                     {"csv_layout", "simple"},
                                     {"frame_column", 0},
                                     {"x_column", 1},
                                     {"y_column", 2},
                                     {"column_delim", ","}}})},
            {"saves",
             nlohmann::json::array({{{"data_key", "tracked_points"},
                                     {"format", "csv"},
                                     {"path", "${output_name}"}}})}};

    {
        std::ofstream output(json_path);
        output << config.dump(2);
    }

    DataManager data_manager;
    auto const result = WhiskerToolbox::DataManagerPipeline::runJsonPipelineFile(
            data_manager,
            json_path.string());

    REQUIRE(result.m_success);
    REQUIRE(result.m_saved_paths.size() == 1);
    REQUIRE(result.m_saved_paths.front() == save_path.lexically_normal().string());
    REQUIRE(std::filesystem::exists(save_path));
}

TEST_CASE("JsonPipelineRunner reports save command failures",
          "[DataManager][JsonPipelineRunner]") {
    TempPipelineDirectory temp_dir;
    nlohmann::json const config = {
            {"saves",
             nlohmann::json::array({{{"data_key", "missing_points"},
                                     {"format", "csv"},
                                     {"path", "missing.csv"}}})}};

    DataManager data_manager;
    auto const result = WhiskerToolbox::DataManagerPipeline::runJsonPipeline(
            data_manager,
            config,
            temp_dir.path().string());

    REQUIRE_FALSE(result.m_success);
    REQUIRE(result.m_failed_phase == WhiskerToolbox::DataManagerPipeline::JsonPipelinePhase::Save);
    REQUIRE(result.m_error_message.find("missing_points") != std::string::npos);
}

TEST_CASE("JsonPipelineRunner reports normalize errors",
          "[DataManager][JsonPipelineRunner]") {
    DataManager data_manager;
    nlohmann::json const scalar_root = 42;
    auto const result = WhiskerToolbox::DataManagerPipeline::runJsonPipeline(
            data_manager,
            scalar_root,
            ".");

    REQUIRE_FALSE(result.m_success);
    REQUIRE(result.m_failed_phase == WhiskerToolbox::DataManagerPipeline::JsonPipelinePhase::Normalize);
    REQUIRE_FALSE(result.m_error_message.empty());
}
