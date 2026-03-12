/**
 * @file SaveDataDigitalEvent.test.cpp
 * @brief Unit tests for SaveData command with DigitalEventSeries CSV format
 *
 * These tests verify the SaveData command correctly saves DigitalEventSeries
 * data through the LoaderRegistry, and that the saved files can be loaded back
 * with data integrity preserved (round-trip).
 */

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/IO/SaveData.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"

#include "IO/core/LoaderRegistration.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/formats/CSV/digitaltimeseries/Digital_Event_Series_CSV.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

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

/// Create a CommandContext with a DigitalEventSeries populated with test events
CommandContext makeContextWithEvents(std::string const & key) {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalEventSeries>(key, TimeKey("time"));

    auto events = dm->getData<DigitalEventSeries>(key);
    events->addEvent(TimeFrameIndex(100));
    events->addEvent(TimeFrameIndex(200));
    events->addEvent(TimeFrameIndex(350));

    CommandContext ctx;
    ctx.data_manager = dm;
    return ctx;
}

std::filesystem::path makeTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "whisker_save_event_test";
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

TEST_CASE("SaveData saves DigitalEventSeries to CSV",
          "[commands][SaveData][DigitalEvent]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithEvents("spikes");

    auto const filepath = (temp_dir / "events.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "spikes",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(std::filesystem::exists(filepath));

    // Verify file has content (header + 3 events)
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

TEST_CASE("SaveData round-trips DigitalEventSeries through CSV",
          "[commands][SaveData][DigitalEvent]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithEvents("spikes");

    auto const filepath = (temp_dir / "roundtrip_events.csv").string();

    SaveData cmd(SaveDataParams{
            .data_key = "spikes",
            .format = "csv",
            .path = filepath,
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Load back using the CSV loader
    CSVEventLoaderOptions load_opts;
    load_opts.filepath = filepath;
    load_opts.has_header = true;// default saver includes header
    load_opts.event_column = 0;

    auto loaded_series = load(load_opts);
    REQUIRE(loaded_series.size() == 1);

    auto const & loaded = *loaded_series[0];
    REQUIRE(loaded.size() == 3);

    // Verify all original event times are present
    auto view = loaded.view();
    std::vector<int64_t> loaded_times;
    for (auto const & event: view) {
        loaded_times.push_back(event.time().getValue());
    }
    std::sort(loaded_times.begin(), loaded_times.end());

    REQUIRE(loaded_times[0] == 100);
    REQUIRE(loaded_times[1] == 200);
    REQUIRE(loaded_times[2] == 350);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Format options (custom delimiter, no header)
// ============================================================================

TEST_CASE("SaveData passes format_options to CSV saver for DigitalEventSeries",
          "[commands][SaveData][DigitalEvent]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithEvents("spikes");

    auto const filepath = (temp_dir / "custom_events.csv").string();

    // Build format_options with custom settings
    auto const opts_json = R"({"save_header": false, "precision": 0})";
    auto format_opts = rfl::json::read<rfl::Generic>(opts_json);
    REQUIRE(format_opts);

    SaveData cmd(SaveDataParams{
            .data_key = "spikes",
            .format = "csv",
            .path = filepath,
            .format_options = format_opts.value(),
    });

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Read file content — should have no header, integers only
    std::ifstream ifs(filepath);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());

    // No header means first line is data
    REQUIRE(content.find("Event") == std::string::npos);

    // Should have exactly 3 lines of data
    auto line_count = std::count(content.begin(), content.end(), '\n');
    REQUIRE(line_count == 3);

    cleanupTempDir(temp_dir);
}

// ============================================================================
// Factory creation
// ============================================================================

TEST_CASE("SaveData for DigitalEventSeries can be created via factory",
          "[commands][SaveData][DigitalEvent][factory]") {
    auto temp_dir = makeTempDir();
    auto ctx = makeContextWithEvents("spikes");

    auto const filepath = (temp_dir / "factory_events.csv").string();

    auto const json = R"({
        "data_key": "spikes",
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

TEST_CASE("SaveData errors when DigitalEventSeries key does not exist",
          "[commands][SaveData][DigitalEvent]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    SaveData cmd(SaveDataParams{
            .data_key = "nonexistent_events",
            .format = "csv",
            .path = "/tmp/should_not_exist.csv",
    });

    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("not found") != std::string::npos);
}
