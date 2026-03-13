/**
 * @file SaveDataDigitalInterval.test.cpp
 * @brief Unit tests for SaveData command with DigitalIntervalSeries CSV format
 *
 * These tests verify the SaveData command correctly saves DigitalIntervalSeries
 * data through the LoaderRegistry, and that the saved files can be loaded back
 * with data integrity preserved (round-trip).
 */

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/IO/SaveData.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "IO/core/LoaderRegistration.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/formats/CSV/digitaltimeseries/Digital_Interval_Series_CSV.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

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

/// Create a CommandContext with a DigitalIntervalSeries populated with test intervals
CommandContext makeContextWithIntervals(std::string const & key) {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>(key, TimeKey("time"));

    auto intervals = dm->getData<DigitalIntervalSeries>(key);
    intervals->addEvent(Interval{100, 150});
    intervals->addEvent(Interval{200, 250});
    intervals->addEvent(Interval{300, 350});

    CommandContext ctx;
    ctx.data_manager = dm;
    return ctx;
}

std::filesystem::path makeTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "whisker_save_interval_test";
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

TEST_CASE("SaveData saves DigitalIntervalSeries to CSV",
          "[commands][SaveData][DigitalInterval]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithIntervals("intervals");

    auto const filepath = (temp_dir / "intervals.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "intervals",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(std::filesystem::exists(filepath));

    // Verify file has content (header + interval rows)
    std::ifstream ifs(filepath);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    REQUIRE_FALSE(content.empty());

    auto line_count = std::count(content.begin(), content.end(), '\n');
    REQUIRE(line_count >= 3);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Round-trip: save then load back
// ============================================================================

TEST_CASE("SaveData round-trips DigitalIntervalSeries through CSV",
          "[commands][SaveData][DigitalInterval]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithIntervals("intervals");

    auto const filepath = (temp_dir / "roundtrip_intervals.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "intervals",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Load back using the CSV loader
    CSVIntervalLoaderOptions load_opts;
    load_opts.filepath = filepath;
    load_opts.has_header = true;// default saver includes header

    auto loaded_intervals = load(load_opts);
    REQUIRE(loaded_intervals.size() == 3);

    // Verify all original intervals are present
    std::sort(loaded_intervals.begin(), loaded_intervals.end(),
              [](Interval const & a, Interval const & b) {
                  if (a.start == b.start) {
                      return a.end < b.end;
                  }
                  return a.start < b.start;
              });

    REQUIRE(loaded_intervals[0].start == 100);
    REQUIRE(loaded_intervals[0].end == 150);
    REQUIRE(loaded_intervals[1].start == 200);
    REQUIRE(loaded_intervals[1].end == 250);
    REQUIRE(loaded_intervals[2].start == 300);
    REQUIRE(loaded_intervals[2].end == 350);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Format options (example: no header)
// ============================================================================

TEST_CASE("SaveData passes format_options to CSV saver for DigitalIntervalSeries",
          "[commands][SaveData][DigitalInterval]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithIntervals("intervals");

    auto const filepath = (temp_dir / "custom_intervals.csv").string();

    // Build format_options with custom settings (mirror event test style)
    auto const opts_json = R"({"save_header": false})";
    auto format_opts = rfl::json::read<rfl::Generic>(opts_json);
    REQUIRE(format_opts);

    SaveData cmd(SaveDataParams{
            .data_key = "intervals",
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

    REQUIRE(content.find("Start") == std::string::npos);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Factory creation
// ============================================================================

TEST_CASE("SaveData for DigitalIntervalSeries can be created via factory",
          "[commands][SaveData][DigitalInterval][factory]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithIntervals("intervals");

    auto const filepath = (temp_dir / "factory_intervals.csv").string();

    auto const json = R"({
        "data_key": "intervals",
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

TEST_CASE("SaveData errors when DigitalIntervalSeries key does not exist",
          "[commands][SaveData][DigitalInterval]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    SaveData cmd(SaveDataParams{
            .data_key = "nonexistent_intervals",
            .format = "csv",
            .path = "/tmp/should_not_exist_intervals.csv",
    });

    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("not found") != std::string::npos);
}
