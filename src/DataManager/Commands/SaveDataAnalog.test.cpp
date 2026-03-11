/**
 * @file SaveDataAnalog.test.cpp
 * @brief Unit tests for SaveData command with AnalogTimeSeries CSV format
 *
 * These tests verify the SaveData command correctly saves AnalogTimeSeries
 * data through the LoaderRegistry, and that the saved files can be loaded back
 * with data integrity preserved (round-trip).
 */

#include "DataManager/Commands/CommandContext.hpp"
#include "DataManager/Commands/CommandFactory.hpp"
#include "DataManager/Commands/SaveData.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"

#include "IO/core/LoaderRegistration.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace commands;

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

/// Create a CommandContext with an AnalogTimeSeries populated with test data
CommandContext makeContextWithAnalog(std::string const & key) {
    auto dm = std::make_shared<DataManager>();
    auto analog = std::make_shared<AnalogTimeSeries>(
            std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f}, 5);
    dm->setData<AnalogTimeSeries>(key, analog, TimeKey("time"));

    CommandContext ctx;
    ctx.data_manager = dm;
    return ctx;
}

std::filesystem::path makeTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "whisker_save_analog_test";
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanupTempDir(std::filesystem::path const & dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

}// namespace

// ============================================================================
// Basic save
// ============================================================================

TEST_CASE("SaveData saves AnalogTimeSeries to CSV",
          "[commands][SaveData][Analog]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithAnalog("signal");

    auto const filepath = (temp_dir / "analog.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "signal",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(std::filesystem::exists(filepath));

    // Verify file has content (header + 5 data rows)
    std::ifstream ifs(filepath);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    REQUIRE_FALSE(content.empty());

    auto line_count = std::count(content.begin(), content.end(), '\n');
    REQUIRE(line_count >= 5);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Round-trip: save then load back
// ============================================================================

TEST_CASE("SaveData round-trips AnalogTimeSeries through CSV",
          "[commands][SaveData][Analog]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithAnalog("signal");

    auto const filepath = (temp_dir / "roundtrip_analog.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "signal",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Load back using the CSV loader
    CSVAnalogLoaderOptions load_opts;
    load_opts.filepath = filepath;
    load_opts.has_header = true;
    load_opts.single_column_format = false;
    load_opts.time_column = 0;
    load_opts.data_column = 1;

    auto loaded = load(load_opts);
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->getNumSamples() == 5);

    auto const data_span = loaded->getAnalogTimeSeries();
    REQUIRE(data_span.size() == 5);
    REQUIRE(data_span[0] == 1.0f);
    REQUIRE(data_span[1] == 2.0f);
    REQUIRE(data_span[2] == 3.0f);
    REQUIRE(data_span[3] == 4.0f);
    REQUIRE(data_span[4] == 5.0f);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Format options (custom delimiter, no header)
// ============================================================================

TEST_CASE("SaveData passes format_options to CSV saver for AnalogTimeSeries",
          "[commands][SaveData][Analog]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithAnalog("signal");

    auto const filepath = (temp_dir / "custom_analog.csv").string();

    // Build format_options with custom settings
    auto const opts_json = R"({"save_header": false, "precision": 0})";
    auto format_opts = rfl::json::read<rfl::Generic>(opts_json);
    REQUIRE(format_opts);

    SaveData cmd(SaveDataParams{
            .data_key = "signal",
            .format = "csv",
            .path = filepath,
            .format_options = format_opts.value(),
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Read file content — should have no header
    std::ifstream ifs(filepath);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());

    // No header means first line is data (no "Time" or "Data" header)
    REQUIRE(content.find("Time") == std::string::npos);
    REQUIRE(content.find("Data") == std::string::npos);

    // Should have exactly 5 lines of data
    auto line_count = std::count(content.begin(), content.end(), '\n');
    REQUIRE(line_count == 5);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Factory creation
// ============================================================================

TEST_CASE("SaveData for AnalogTimeSeries can be created via factory",
          "[commands][SaveData][Analog][factory]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithAnalog("signal");

    auto const filepath = (temp_dir / "factory_analog.csv").string();

    auto const json = R"({
        "data_key": "signal",
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

// ============================================================================
// Error case: nonexistent data key
// ============================================================================

TEST_CASE("SaveData errors when AnalogTimeSeries key does not exist",
          "[commands][SaveData][Analog]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    SaveData cmd(SaveDataParams{
            .data_key = "nonexistent_signal",
            .format = "csv",
            .path = "/tmp/should_not_exist.csv",
    });

    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("not found") != std::string::npos);
}
