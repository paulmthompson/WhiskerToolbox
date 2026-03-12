/**
 * @file SaveData.test.cpp
 * @brief Unit tests for the SaveData command
 */

#include "Commands/IO/SaveData.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"

#include "DataManager/DataManager.hpp"
#include "Points/Point_Data.hpp"

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

CommandContext makeContextWithPoints(std::string const & key) {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>(key, TimeKey("time"));

    auto pts = dm->getData<PointData>(key);
    pts->addEntryAtTime(TimeFrameIndex(0),
                        Point2D<float>{1.0f, 2.0f},
                        EntityId(1), NotifyObservers::No);
    pts->addEntryAtTime(TimeFrameIndex(5),
                        Point2D<float>{3.0f, 4.0f},
                        EntityId(2), NotifyObservers::No);
    pts->addEntryAtTime(TimeFrameIndex(10),
                        Point2D<float>{5.0f, 6.0f},
                        EntityId(3), NotifyObservers::No);

    CommandContext ctx;
    ctx.data_manager = dm;
    return ctx;
}

/// Create a unique temp directory and return its path
std::filesystem::path makeTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "whisker_save_data_test";
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

TEST_CASE("createCommand creates SaveData from valid params",
          "[commands][SaveData][factory]") {
    auto const json = R"({
        "data_key": "my_points",
        "format": "csv",
        "path": "/tmp/test_output.csv"
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("SaveData", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "SaveData");
    REQUIRE_FALSE(cmd->isUndoable());
}

TEST_CASE("createCommand creates SaveData with format_options",
          "[commands][SaveData][factory]") {
    auto const json = R"({
        "data_key": "my_points",
        "format": "csv",
        "path": "/tmp/test_output.csv",
        "format_options": {"delimiter": ";"}
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("SaveData", generic);
    REQUIRE(cmd != nullptr);
}

TEST_CASE("createCommand returns nullptr for invalid SaveData params",
          "[commands][SaveData][factory]") {
    auto const json = R"({"not_a_valid_field": 42})";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("SaveData", generic);
    REQUIRE(cmd == nullptr);
}

TEST_CASE("isKnownCommandName recognizes SaveData",
          "[commands][SaveData][factory]") {
    REQUIRE(isKnownCommandName("SaveData"));
}

// ============================================================================
// Execution tests
// ============================================================================

TEST_CASE("SaveData saves PointData to CSV file",
          "[commands][SaveData]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithPoints("pts");

    auto const filepath = (temp_dir / "points.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "pts",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Verify the file was created
    REQUIRE(std::filesystem::exists(filepath));

    // Verify the file has content
    std::ifstream ifs(filepath);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    REQUIRE_FALSE(content.empty());

    // Verify it contains the expected data (3 points + header)
    auto line_count = std::count(content.begin(), content.end(), '\n');
    REQUIRE(line_count >= 3);// 3 data lines (plus possibly a header)

    cleanupTempDir(temp_dir);
}

TEST_CASE("SaveData round-trips PointData through CSV",
          "[commands][SaveData]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithPoints("pts");

    auto const filepath = (temp_dir / "roundtrip.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "pts",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Load back via direct loader (CSV saver uses comma delimiter by default)
    CSVPointLoaderOptions load_opts;
    load_opts.filepath = filepath;
    load_opts.column_delim = ",";
    auto loaded = load(load_opts);

    // Original had 3 frames with one point each
    REQUIRE(loaded.size() == 3);
    REQUIRE(loaded.count(TimeFrameIndex(0)) == 1);
    REQUIRE(loaded.count(TimeFrameIndex(5)) == 1);
    REQUIRE(loaded.count(TimeFrameIndex(10)) == 1);

    cleanupTempDir(temp_dir);
}

TEST_CASE("SaveData errors on unknown data_key",
          "[commands][SaveData]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    SaveData cmd(SaveDataParams{
            .data_key = "nonexistent",
            .format = "csv",
            .path = "/tmp/should_not_exist.csv",
    });

    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("not found") != std::string::npos);
}

TEST_CASE("SaveData errors on unsupported format",
          "[commands][SaveData]") {
    auto ctx = makeContextWithPoints("pts");

    SaveData cmd(SaveDataParams{
            .data_key = "pts",
            .format = "totally_fake_format",
            .path = "/tmp/should_not_exist.xyz",
    });

    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("SaveData toJson produces valid JSON",
          "[commands][SaveData]") {
    SaveData cmd(SaveDataParams{
            .data_key = "my_points",
            .format = "csv",
            .path = "/tmp/output.csv",
    });

    auto const json_str = cmd.toJson();
    REQUIRE_FALSE(json_str.empty());

    // Should be parseable
    auto parsed = rfl::json::read<SaveDataParams>(json_str);
    REQUIRE(parsed);

    auto const & val = parsed.value();
    REQUIRE(val.data_key == "my_points");
    REQUIRE(val.format == "csv");
    REQUIRE(val.path == "/tmp/output.csv");
}

TEST_CASE("SaveData in a command sequence via factory",
          "[commands][SaveData]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithPoints("pts");

    auto const filepath = (temp_dir / "factory_save.csv").string();

    // Create via factory (as would happen in a pipeline)
    auto const json = R"({
        "data_key": "pts",
        "format": "csv",
        "path": ")" + filepath +
                      R"("
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("SaveData", generic);
    REQUIRE(cmd != nullptr);

    auto result = cmd->execute(ctx);
    REQUIRE(result.success);
    REQUIRE(std::filesystem::exists(filepath));

    cleanupTempDir(temp_dir);
}

TEST_CASE("getCommandInfo returns metadata for SaveData",
          "[commands][SaveData][factory]") {
    auto info = getCommandInfo("SaveData");
    REQUIRE(info.has_value());
    REQUIRE(info->name == "SaveData");
    REQUIRE(info->category == "persistence");
    REQUIRE_FALSE(info->supports_undo);
    REQUIRE_FALSE(info->parameter_schema.fields.empty());
}
