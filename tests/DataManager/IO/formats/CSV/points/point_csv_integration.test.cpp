/**
 * @file point_csv_integration.test.cpp
 * @brief Integration tests for loading PointData from CSV via DataManager JSON config
 * 
 * Tests the following scenarios:
 * 1. Simple CSV with frame, x, y columns
 * 2. Custom delimiters (tab, semicolon, space)
 * 3. Custom column indices
 * 4. DLC format with single bodypart
 * 5. DLC format batch loading (all bodyparts via batch loader interface)
 * 6. Various edge cases (single point, negative coords, precision, etc.)
 * 
 * Uses builder-based scenarios to create test data, writes to temporary
 * CSV files, then loads via DataManager JSON config.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/points/csv_point_loading_scenarios.hpp"

#include "DataManager.hpp"
#include "Points/Point_Data.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/core/LoaderRegistration.hpp"
#include "IO/formats/CSV/CSVLoader.hpp"

#include <nlohmann/json.hpp>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>

using json = nlohmann::json;

/**
 * @brief Helper class for managing temporary test directories
 */
class TempCSVPointTestDirectory {
public:
    TempCSVPointTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_csv_point_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempCSVPointTestDirectory() {
        if (std::filesystem::exists(temp_path)) {
            std::filesystem::remove_all(temp_path);
        }
    }
    
    std::filesystem::path getPath() const { return temp_path; }
    
    std::string getPathString() const { return temp_path.string(); }
    
    std::filesystem::path getFilePath(std::string const& filename) const {
        return temp_path / filename;
    }

private:
    std::filesystem::path temp_path;
};

/**
 * @brief Helper to verify point data equality between original and loaded data
 * 
 * @param original Original point data as a map
 * @param loaded Loaded PointData object
 * @param tolerance Float comparison tolerance
 */
void verifyPointsEqual(std::map<TimeFrameIndex, Point2D<float>> const& original, 
                       PointData const& loaded,
                       float tolerance = 0.01f) {
    auto loaded_time_count = loaded.getTimeCount();
    REQUIRE(loaded_time_count == original.size());
    
    for (auto const& [time, point] : original) {
        // Use getAtTime which returns a range of data at that time
        auto points_at_time = loaded.getAtTime(time);
        
        // Collect points into a vector for easier checking
        std::vector<Point2D<float>> loaded_points;
        for (auto const& p : points_at_time) {
            loaded_points.push_back(p);
        }
        
        REQUIRE(loaded_points.size() == 1);
        REQUIRE(loaded_points[0].x == Catch::Approx(point.x).epsilon(tolerance));
        REQUIRE(loaded_points[0].y == Catch::Approx(point.y).epsilon(tolerance));
    }
}

//=============================================================================
// Test Case 1: Simple CSV with Header (frame, x, y)
//=============================================================================

TEST_CASE("PointData CSV Integration - Simple CSV with Header", 
          "[points][csv][integration][datamanager]") {
    TempCSVPointTestDirectory temp_dir;
    
    SECTION("Simple points with comma delimiter") {
        auto original = point_csv_scenarios::simple_points();
        
        auto csv_path = temp_dir.getFilePath("simple_points.csv");
        REQUIRE(point_csv_scenarios::writeCSVSimple(original, csv_path.string()));
        
        // Create JSON config for simple CSV loading
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "test_simple_points"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "simple"},
                {"frame_column", 0},
                {"x_column", 1},
                {"y_column", 2},
                {"column_delim", ","}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<PointData>("test_simple_points");
        REQUIRE(loaded != nullptr);
        
        verifyPointsEqual(original, *loaded);
    }
    
    SECTION("Single point") {
        auto original = point_csv_scenarios::single_point();
        
        auto csv_path = temp_dir.getFilePath("single_point.csv");
        REQUIRE(point_csv_scenarios::writeCSVSimple(original, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "single_point"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "simple"},
                {"frame_column", 0},
                {"x_column", 1},
                {"y_column", 2},
                {"column_delim", ","}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<PointData>("single_point");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getTimeCount() == 1);
        
        verifyPointsEqual(original, *loaded);
    }
    
    SECTION("Dense sequential points") {
        auto original = point_csv_scenarios::dense_points();
        
        auto csv_path = temp_dir.getFilePath("dense_points.csv");
        REQUIRE(point_csv_scenarios::writeCSVSimple(original, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "dense_points"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "simple"},
                {"frame_column", 0},
                {"x_column", 1},
                {"y_column", 2},
                {"column_delim", ","}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<PointData>("dense_points");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getTimeCount() == 10);
        
        verifyPointsEqual(original, *loaded);
    }
}

//=============================================================================
// Test Case 2: CSV without Header / Space-Delimited
//=============================================================================

TEST_CASE("PointData CSV Integration - No Header / Space Delimiter", 
          "[points][csv][integration][datamanager]") {
    TempCSVPointTestDirectory temp_dir;
    
    SECTION("Space-delimited no header (typical format)") {
        auto original = point_csv_scenarios::simple_points();
        
        auto csv_path = temp_dir.getFilePath("space_delim_points.csv");
        REQUIRE(point_csv_scenarios::writeCSVSpaceDelimited(original, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "space_delim_points"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "simple"},
                {"frame_column", 0},
                {"x_column", 1},
                {"y_column", 2},
                {"column_delim", " "}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<PointData>("space_delim_points");
        REQUIRE(loaded != nullptr);
        
        verifyPointsEqual(original, *loaded);
    }
}

//=============================================================================
// Test Case 3: Negative and Decimal Coordinates
//=============================================================================

TEST_CASE("PointData CSV Integration - Negative and Decimal Coordinates", 
          "[points][csv][integration][datamanager]") {
    TempCSVPointTestDirectory temp_dir;
    
    SECTION("Negative coordinates") {
        auto original = point_csv_scenarios::negative_coord_points();
        
        auto csv_path = temp_dir.getFilePath("negative_coords.csv");
        REQUIRE(point_csv_scenarios::writeCSVSimple(original, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "negative_coords"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "simple"},
                {"frame_column", 0},
                {"x_column", 1},
                {"y_column", 2},
                {"column_delim", ","}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<PointData>("negative_coords");
        REQUIRE(loaded != nullptr);
        
        verifyPointsEqual(original, *loaded);
    }
    
    SECTION("Decimal precision coordinates") {
        auto original = point_csv_scenarios::decimal_precision_points();
        
        auto csv_path = temp_dir.getFilePath("decimal_precision.csv");
        REQUIRE(point_csv_scenarios::writeCSVSimple(original, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "decimal_precision"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "simple"},
                {"frame_column", 0},
                {"x_column", 1},
                {"y_column", 2},
                {"column_delim", ","}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<PointData>("decimal_precision");
        REQUIRE(loaded != nullptr);
        
        // Use higher tolerance for decimal precision test
        verifyPointsEqual(original, *loaded, 0.001f);
    }
}

//=============================================================================
// Test Case 4: DLC Format - Single Bodypart Loading via CSVLoader
//=============================================================================

TEST_CASE("PointData CSV Integration - DLC Format Single Bodypart", 
          "[points][csv][dlc][integration][datamanager]") {
    TempCSVPointTestDirectory temp_dir;
    
    // Ensure loaders are registered
    static bool loaders_registered = false;
    if (!loaders_registered) {
        registerAllLoaders();
        loaders_registered = true;
    }
    
    SECTION("Load single bodypart directly via CSVLoader") {
        auto dlc_data = point_csv_scenarios::two_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_two_bodyparts.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        // Load only the "nose" bodypart via direct loader call
        json config;
        config["format"] = "dlc_csv";
        config["bodypart"] = "nose";
        config["likelihood_threshold"] = 0.0;
        
        LoadResult result = loader.load(csv_path.string(), IODataType::Points, config);
        
        REQUIRE(result.success);
        REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(result.data));
        
        auto loaded = std::get<std::shared_ptr<PointData>>(result.data);
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getTimeCount() == 5);
        
        // Verify the nose points match
        verifyPointsEqual(dlc_data.at("nose"), *loaded);
    }
    
    SECTION("Load specific bodypart via CSVLoader from multi-bodypart DLC") {
        auto dlc_data = point_csv_scenarios::three_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_three_bodyparts.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        // Load only the "body" bodypart
        json config;
        config["format"] = "dlc_csv";
        config["bodypart"] = "body";
        config["likelihood_threshold"] = 0.0;
        
        LoadResult result = loader.load(csv_path.string(), IODataType::Points, config);
        
        REQUIRE(result.success);
        auto loaded = std::get<std::shared_ptr<PointData>>(result.data);
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getTimeCount() == 3);
        
        verifyPointsEqual(dlc_data.at("body"), *loaded);
    }
    
    SECTION("DataManager with DLC loads all bodyparts with bodypart suffix") {
        // NOTE: When using DataManager JSON config with dlc_csv format,
        // ALL bodyparts are loaded, and each is stored with key "{name}_{bodypart}"
        auto dlc_data = point_csv_scenarios::two_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_dm_test.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "dlc_points"},
                {"filepath", csv_path.string()},
                {"format", "dlc_csv"},
                {"likelihood_threshold", 0.0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Both bodyparts should be loaded with suffix
        auto nose_loaded = dm.getData<PointData>("dlc_points_nose");
        auto tail_loaded = dm.getData<PointData>("dlc_points_tail");
        
        REQUIRE(nose_loaded != nullptr);
        REQUIRE(tail_loaded != nullptr);
        REQUIRE(nose_loaded->getTimeCount() == 5);
        REQUIRE(tail_loaded->getTimeCount() == 5);
        
        verifyPointsEqual(dlc_data.at("nose"), *nose_loaded);
        verifyPointsEqual(dlc_data.at("tail"), *tail_loaded);
    }
}

//=============================================================================
// Test Case 5: DLC Format - Batch Loading via Batch Loader Interface
//=============================================================================

TEST_CASE("PointData CSV Integration - DLC Batch Loading via Registry", 
          "[points][csv][dlc][batch][integration]") {
    TempCSVPointTestDirectory temp_dir;
    
    // Ensure loaders are registered
    static bool loaders_registered = false;
    if (!loaders_registered) {
        registerAllLoaders();
        loaders_registered = true;
    }
    
    SECTION("CSVLoader directly - batch load all bodyparts") {
        auto dlc_data = point_csv_scenarios::two_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_batch_test.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        // Verify batch loading is supported for DLC format
        REQUIRE(loader.supportsBatchLoading("dlc_csv", IODataType::Points));
        
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        REQUIRE(result.results.size() == 2);  // nose and tail
        
        // Check that all results have expected bodypart names
        std::set<std::string> expected_names = {"nose", "tail"};
        std::set<std::string> loaded_names;
        
        for (auto const& res : result.results) {
            REQUIRE(res.success);
            REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(res.data));
            loaded_names.insert(res.name);
            
            auto point_data = std::get<std::shared_ptr<PointData>>(res.data);
            REQUIRE(point_data != nullptr);
            REQUIRE(point_data->getTimeCount() == 5);
        }
        
        REQUIRE(loaded_names == expected_names);
    }
    
    SECTION("CSVLoader batch load with three bodyparts") {
        auto dlc_data = point_csv_scenarios::three_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_three_bodyparts_batch.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        REQUIRE(result.results.size() == 3);  // head, body, tail
        
        std::set<std::string> expected_names = {"head", "body", "tail"};
        std::set<std::string> loaded_names;
        
        for (auto const& res : result.results) {
            REQUIRE(res.success);
            loaded_names.insert(res.name);
            
            auto point_data = std::get<std::shared_ptr<PointData>>(res.data);
            REQUIRE(point_data != nullptr);
            REQUIRE(point_data->getTimeCount() == 3);  // Each has 3 frames
        }
        
        REQUIRE(loaded_names == expected_names);
    }
    
    SECTION("Batch load verifies point data matches original") {
        auto dlc_data = point_csv_scenarios::two_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_verify_data.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        
        for (auto const& res : result.results) {
            auto point_data = std::get<std::shared_ptr<PointData>>(res.data);
            
            // Get original data for this bodypart
            auto original_it = dlc_data.find(res.name);
            REQUIRE(original_it != dlc_data.end());
            
            verifyPointsEqual(original_it->second, *point_data);
        }
    }
}

//=============================================================================
// Test Case 6: DLC Format - Batch Loading via LoaderRegistry
//=============================================================================

TEST_CASE("PointData CSV Integration - DLC via LoaderRegistry Batch Loading", 
          "[points][csv][dlc][batch][registry][integration]") {
    TempCSVPointTestDirectory temp_dir;
    
    // Ensure loaders are registered
    static bool loaders_registered = false;
    if (!loaders_registered) {
        registerAllLoaders();
        loaders_registered = true;
    }
    
    SECTION("LoaderRegistry supports batch loading for DLC") {
        auto& registry = LoaderRegistry::getInstance();
        
        REQUIRE(registry.isBatchLoadingSupported("dlc_csv", IODataType::Points));
    }
    
    SECTION("tryLoadBatch for DLC format") {
        auto dlc_data = point_csv_scenarios::two_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_registry_batch.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        auto& registry = LoaderRegistry::getInstance();
        auto batch_result = registry.tryLoadBatch(
            "dlc_csv",
            IODataType::Points, 
            csv_path.string(), 
            config
        );
        
        REQUIRE(batch_result.success);
        REQUIRE(batch_result.results.size() == 2);
    }
}

//=============================================================================
// Test Case 7: DLC Format - All Bodyparts Loading via DataManager JSON Config
//=============================================================================

TEST_CASE("PointData CSV Integration - DLC All Bodyparts via DataManager", 
          "[points][csv][dlc][batch][datamanager][integration]") {
    TempCSVPointTestDirectory temp_dir;
    
    SECTION("Load all bodyparts from DLC via JSON config") {
        auto dlc_data = point_csv_scenarios::two_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_all_bodyparts.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        // Note: The current DataManager implementation has a specialized path for dlc_csv
        // that calls load_multiple_PointData_from_dlc() directly
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "dlc_points"},
                {"filepath", csv_path.string()},
                {"format", "dlc_csv"},
                {"likelihood_threshold", 0.0}
            }
        });
        
        DataManager dm;
        auto loaded_info = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should have loaded 2 PointData objects (one per bodypart)
        auto point_keys = dm.getKeys<PointData>();
        REQUIRE(point_keys.size() == 2);
        
        // Both bodyparts should be in the data manager
        // Note: Names are prefixed with the base name and bodypart suffix
        bool has_nose = false;
        bool has_tail = false;
        for (auto const& key : point_keys) {
            if (key.find("nose") != std::string::npos) has_nose = true;
            if (key.find("tail") != std::string::npos) has_tail = true;
        }
        REQUIRE(has_nose);
        REQUIRE(has_tail);
    }
    
    SECTION("All bodyparts have correct data") {
        auto dlc_data = point_csv_scenarios::three_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_verify_all.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "multi_point"},
                {"filepath", csv_path.string()},
                {"format", "dlc_csv"},
                {"likelihood_threshold", 0.0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto point_keys = dm.getKeys<PointData>();
        REQUIRE(point_keys.size() == 3);  // head, body, tail
        
        // Verify each bodypart's data
        for (auto const& key : point_keys) {
            auto point_data = dm.getData<PointData>(key);
            REQUIRE(point_data != nullptr);
            REQUIRE(point_data->getTimeCount() == 3);  // Each bodypart has 3 frames
        }
    }
}

//=============================================================================
// Test Case 8: DLC Likelihood Threshold Filtering
//=============================================================================

TEST_CASE("PointData CSV Integration - DLC Likelihood Filtering", 
          "[points][csv][dlc][likelihood][integration]") {
    TempCSVPointTestDirectory temp_dir;
    
    SECTION("High likelihood threshold filters low-confidence points") {
        auto dlc_data = point_csv_scenarios::dlc_with_likelihoods();
        
        auto csv_path = temp_dir.getFilePath("dlc_likelihood.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormatWithLikelihood(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        // Load with high threshold (0.8) - should filter many points
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.8;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        
        // With 0.8 threshold:
        // - nose: frames 0 (0.99), 1 (0.85), 4 (0.95) = 3 points
        // - ear: frames 0 (0.92), 2 (0.88) = 2 points
        for (auto const& res : result.results) {
            auto point_data = std::get<std::shared_ptr<PointData>>(res.data);
            
            if (res.name == "nose") {
                REQUIRE(point_data->getTimeCount() == 3);
            } else if (res.name == "ear") {
                REQUIRE(point_data->getTimeCount() == 2);
            }
        }
    }
    
    SECTION("Zero threshold includes all points") {
        auto dlc_data = point_csv_scenarios::dlc_with_likelihoods();
        
        auto csv_path = temp_dir.getFilePath("dlc_zero_threshold.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormatWithLikelihood(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        
        for (auto const& res : result.results) {
            auto point_data = std::get<std::shared_ptr<PointData>>(res.data);
            REQUIRE(point_data->getTimeCount() == 5);  // All 5 frames
        }
    }
}

//=============================================================================
// Test Case 9: Edge Cases - Sparse Data and Large Gaps
//=============================================================================

TEST_CASE("PointData CSV Integration - Sparse Data and Large Time Gaps", 
          "[points][csv][integration][edge-cases]") {
    TempCSVPointTestDirectory temp_dir;
    
    SECTION("Sparse points with large frame gaps") {
        auto original = point_csv_scenarios::sparse_points();
        
        auto csv_path = temp_dir.getFilePath("sparse_points.csv");
        REQUIRE(point_csv_scenarios::writeCSVSimple(original, csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "points"},
                {"name", "sparse_points"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "simple"},
                {"frame_column", 0},
                {"x_column", 1},
                {"y_column", 2},
                {"column_delim", ","}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<PointData>("sparse_points");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getTimeCount() == 3);
        
        verifyPointsEqual(original, *loaded);
        
        // Verify specific frame values using getAtTime
        auto points_at_5000_view = loaded->getAtTime(TimeFrameIndex(5000));
        std::vector<Point2D<float>> points_at_5000(points_at_5000_view.begin(), points_at_5000_view.end());
        REQUIRE(points_at_5000.size() == 1);
        REQUIRE(points_at_5000[0].x == Catch::Approx(500.0f).epsilon(0.01));
    }
}

//=============================================================================
// Test Case 10: Single Bodypart DLC (Minimal Case)
//=============================================================================

TEST_CASE("PointData CSV Integration - Single Bodypart DLC", 
          "[points][csv][dlc][integration]") {
    TempCSVPointTestDirectory temp_dir;
    
    SECTION("Single bodypart DLC loads correctly") {
        auto dlc_data = point_csv_scenarios::single_bodypart_dlc();
        
        auto csv_path = temp_dir.getFilePath("dlc_single.csv");
        REQUIRE(point_csv_scenarios::writeDLCFormat(dlc_data, csv_path.string()));
        
        CSVLoader loader;
        
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        REQUIRE(result.results.size() == 1);
        REQUIRE(result.results[0].name == "point");
        
        auto point_data = std::get<std::shared_ptr<PointData>>(result.results[0].data);
        REQUIRE(point_data != nullptr);
        REQUIRE(point_data->getTimeCount() == 3);
        
        verifyPointsEqual(dlc_data.at("point"), *point_data);
    }
}
