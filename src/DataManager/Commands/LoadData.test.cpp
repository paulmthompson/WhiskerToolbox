/**
 * @file LoadData.test.cpp
 * @brief Unit tests for the LoadData command
 */

#include "DataManager/Commands/LoadData.hpp"
#include "DataManager/Commands/CommandContext.hpp"
#include "DataManager/Commands/CommandFactory.hpp"
#include "DataManager/Commands/SaveData.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "IO/core/LoaderRegistration.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/formats/CSV/points/Point_Data_CSV.hpp"

#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

#include <filesystem>
#include <fstream>

using namespace commands;

// ============================================================================
// Helper: ensure internal loaders are registered on the global singleton
// ============================================================================
namespace {

struct RegistryInitializer {
    RegistryInitializer() {
        static bool initialized = false;
        if (!initialized) {
            registerInternalLoaders();
            initialized = true;
        }
    }
};

[[maybe_unused]] RegistryInitializer const g_init{};

/// Create a CSV file with known point data and return its path
std::filesystem::path writeTestPointCSV(std::filesystem::path const & dir) {
    auto const filepath = dir / "test_points.csv";
    std::ofstream ofs(filepath);
    ofs << "0 1.0 2.0\n";
    ofs << "5 3.0 4.0\n";
    ofs << "10 5.0 6.0\n";
    return filepath;
}

/// Create a unique temp directory and return its path
std::filesystem::path makeTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "whisker_load_data_test";
    std::filesystem::create_directories(dir);
    return dir;
}

/// Remove the temp directory if it exists
void cleanupTempDir(std::filesystem::path const & dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

}// namespace

// ============================================================================
// Factory tests
// ============================================================================

TEST_CASE("createCommand creates LoadData from valid params",
          "[commands][LoadData][factory]") {
    auto const json = R"({
        "data_key": "my_points",
        "data_type": "PointData",
        "filepath": "/tmp/test.csv",
        "format": "csv"
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("LoadData", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "LoadData");
    REQUIRE(cmd->isUndoable());
}

TEST_CASE("createCommand creates LoadData with format_options",
          "[commands][LoadData][factory]") {
    auto const json = R"({
        "data_key": "my_points",
        "data_type": "PointData",
        "filepath": "/tmp/test.csv",
        "format": "csv",
        "format_options": {"column_delim": ";"}
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("LoadData", generic);
    REQUIRE(cmd != nullptr);
}

TEST_CASE("createCommand returns nullptr for invalid LoadData params",
          "[commands][LoadData][factory]") {
    auto const json = R"({"not_a_valid_field": 42})";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("LoadData", generic);
    REQUIRE(cmd == nullptr);
}

TEST_CASE("isKnownCommandName recognizes LoadData",
          "[commands][LoadData][factory]") {
    REQUIRE(isKnownCommandName("LoadData"));
}

// ============================================================================
// Execution tests
// ============================================================================

TEST_CASE("LoadData loads PointData from CSV file",
          "[commands][LoadData]") {
    auto temp_dir = makeTempDir();
    auto const filepath = writeTestPointCSV(temp_dir);

    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;

    LoadData cmd(LoadDataParams{
            .data_key = "loaded_pts",
            .data_type = "PointData",
            .filepath = filepath.string(),
            .format = "csv",
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(result.affected_keys[0] == "loaded_pts");

    // Verify data was loaded into DataManager
    auto pts = dm->getData<PointData>("loaded_pts");
    REQUIRE(pts != nullptr);

    // Verify the data content
    REQUIRE(pts->getTimeCount() == 3);

    cleanupTempDir(temp_dir);
}

TEST_CASE("LoadData errors on unknown data_type",
          "[commands][LoadData]") {
    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;

    LoadData cmd(LoadDataParams{
            .data_key = "pts",
            .data_type = "UnknownType",
            .filepath = "/tmp/nonexistent.csv",
            .format = "csv",
    });

    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("Unknown data_type") != std::string::npos);
}

TEST_CASE("LoadData with nonexistent file either fails or loads empty data",
          "[commands][LoadData]") {
    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;

    LoadData cmd(LoadDataParams{
            .data_key = "pts",
            .data_type = "PointData",
            .filepath = "/tmp/definitely_does_not_exist_12345.csv",
            .format = "csv",
    });

    auto result = cmd.execute(ctx);
    // Behavior is loader-dependent: some loaders fail, others return empty data
    if (result.success) {
        auto pts = dm->getData<PointData>("pts");
        REQUIRE(pts != nullptr);
        REQUIRE(pts->getTimeCount() == 0);
    } else {
        REQUIRE_FALSE(result.error_message.empty());
    }
}

TEST_CASE("LoadData errors on unknown format",
          "[commands][LoadData]") {
    auto temp_dir = makeTempDir();
    auto const filepath = writeTestPointCSV(temp_dir);

    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;

    LoadData cmd(LoadDataParams{
            .data_key = "pts",
            .data_type = "PointData",
            .filepath = filepath.string(),
            .format = "totally_fake_format",
    });

    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);

    cleanupTempDir(temp_dir);
}

TEST_CASE("LoadData undo removes loaded data",
          "[commands][LoadData]") {
    auto temp_dir = makeTempDir();
    auto const filepath = writeTestPointCSV(temp_dir);

    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;

    LoadData cmd(LoadDataParams{
            .data_key = "loaded_pts",
            .data_type = "PointData",
            .filepath = filepath.string(),
            .format = "csv",
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(dm->getData<PointData>("loaded_pts") != nullptr);

    // Undo should remove the data
    auto undo_result = cmd.undo(ctx);
    REQUIRE(undo_result.success);
    REQUIRE(dm->getData<PointData>("loaded_pts") == nullptr);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Serialization tests
// ============================================================================

TEST_CASE("LoadData toJson produces valid JSON",
          "[commands][LoadData]") {
    LoadData const cmd(LoadDataParams{
            .data_key = "my_points",
            .data_type = "PointData",
            .filepath = "/tmp/input.csv",
            .format = "csv",
    });

    auto const json_str = cmd.toJson();
    REQUIRE_FALSE(json_str.empty());

    // Should be parseable
    auto parsed = rfl::json::read<LoadDataParams>(json_str);
    REQUIRE(parsed);

    auto const & val = parsed.value();
    REQUIRE(val.data_key == "my_points");
    REQUIRE(val.data_type == "PointData");
    REQUIRE(val.filepath == "/tmp/input.csv");
    REQUIRE(val.format == "csv");
}

// ============================================================================
// Integration tests
// ============================================================================

TEST_CASE("LoadData in a command sequence via factory",
          "[commands][LoadData]") {
    auto temp_dir = makeTempDir();
    auto const filepath = writeTestPointCSV(temp_dir);

    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;

    // Create via factory (as would happen in a pipeline)
    auto const json = R"({
        "data_key": "factory_pts",
        "data_type": "PointData",
        "filepath": ")" +
                      filepath.string() +
                      R"(",
        "format": "csv"
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("LoadData", generic);
    REQUIRE(cmd != nullptr);

    auto result = cmd->execute(ctx);
    REQUIRE(result.success);
    REQUIRE(dm->getData<PointData>("factory_pts") != nullptr);

    cleanupTempDir(temp_dir);
}

TEST_CASE("getCommandInfo returns metadata for LoadData",
          "[commands][LoadData][factory]") {
    auto info = getCommandInfo("LoadData");
    REQUIRE(info.has_value());
    REQUIRE(info->name == "LoadData");
    REQUIRE(info->category == "persistence");
    REQUIRE(info->supports_undo);
    REQUIRE_FALSE(info->parameter_schema.fields.empty());
}

TEST_CASE("LoadData + SaveData round-trip via command sequence",
          "[commands][LoadData]") {
    auto temp_dir = makeTempDir();
    auto const input_filepath = writeTestPointCSV(temp_dir);
    auto const output_filepath = (temp_dir / "roundtrip_output.csv").string();

    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;

    // Load
    LoadData load_cmd(LoadDataParams{
            .data_key = "roundtrip_pts",
            .data_type = "PointData",
            .filepath = input_filepath.string(),
            .format = "csv",
    });

    auto load_result = load_cmd.execute(ctx);
    REQUIRE(load_result.success);

    // Save
    SaveData save_cmd(SaveDataParams{
            .data_key = "roundtrip_pts",
            .format = "csv",
            .path = output_filepath,
    });

    auto save_result = save_cmd.execute(ctx);
    REQUIRE(save_result.success);

    // Verify saved file exists and has content
    REQUIRE(std::filesystem::exists(output_filepath));
    std::ifstream ifs(output_filepath);
    std::string const content((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    REQUIRE_FALSE(content.empty());

    cleanupTempDir(temp_dir);
}
