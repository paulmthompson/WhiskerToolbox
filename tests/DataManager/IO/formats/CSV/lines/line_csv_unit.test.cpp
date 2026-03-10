/**
 * @file line_csv_unit.test.cpp
 * @brief Unit tests for Line CSV save/load round-trip
 *
 * Tests deterministic round-trips for both single-file and multi-file
 * CSV line data formats with known data.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "IO/formats/CSV/lines/Line_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

/// Helper to verify two Line2D objects match within tolerance
void verifyLineMatch(Line2D const & expected, Line2D const & actual, float tolerance = 0.01f) {
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        REQUIRE_THAT(actual[i].x, WithinAbs(expected[i].x, tolerance));
        REQUIRE_THAT(actual[i].y, WithinAbs(expected[i].y, tolerance));
    }
}

}// namespace

TEST_CASE("DM - IO - LineData CSV Unit - Single file round-trip", "[LineData][CSV][IO][unit]") {

    auto const test_dir = std::filesystem::temp_directory_path() / "line_csv_unit_single";
    std::filesystem::create_directories(test_dir);

    SECTION("Single point line") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(5)].push_back(Line2D{{10.5f, 20.3f}});

        LineData const line_data(input);

        CSVSingleFileLineSaverOptions save_opts;
        save_opts.filename = "single_point.csv";
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 2;

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        CSVSingleFileLineLoaderOptions load_opts;
        load_opts.filepath = (test_dir / "single_point.csv").string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded.count(TimeFrameIndex(5)) == 1);
        REQUIRE(loaded[TimeFrameIndex(5)].size() == 1);
        verifyLineMatch(input[TimeFrameIndex(5)][0], loaded[TimeFrameIndex(5)][0]);
    }

    SECTION("Multiple points per line") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        Line2D const line = {{1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f}, {7.0f, 8.0f}};
        input[TimeFrameIndex(0)].push_back(line);

        LineData const line_data(input);

        CSVSingleFileLineSaverOptions save_opts;
        save_opts.filename = "multi_points.csv";
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 1;

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        CSVSingleFileLineLoaderOptions load_opts;
        load_opts.filepath = (test_dir / "multi_points.csv").string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded[TimeFrameIndex(0)].size() == 1);
        verifyLineMatch(line, loaded[TimeFrameIndex(0)][0]);
    }

    SECTION("Multiple frames with multiple lines") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(0)].push_back(Line2D{{10.0f, 20.0f}, {30.0f, 40.0f}});
        input[TimeFrameIndex(0)].push_back(Line2D{{100.0f, 200.0f}, {300.0f, 400.0f}});
        input[TimeFrameIndex(5)].push_back(Line2D{{-1.5f, -2.5f}, {-3.5f, -4.5f}, {-5.5f, -6.5f}});

        LineData const line_data(input);

        CSVSingleFileLineSaverOptions save_opts;
        save_opts.filename = "multi_frame.csv";
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 1;

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        CSVSingleFileLineLoaderOptions load_opts;
        load_opts.filepath = (test_dir / "multi_frame.csv").string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 2);
        REQUIRE(loaded[TimeFrameIndex(0)].size() == 2);
        REQUIRE(loaded[TimeFrameIndex(5)].size() == 1);

        verifyLineMatch(input[TimeFrameIndex(0)][0], loaded[TimeFrameIndex(0)][0]);
        verifyLineMatch(input[TimeFrameIndex(0)][1], loaded[TimeFrameIndex(0)][1]);
        verifyLineMatch(input[TimeFrameIndex(5)][0], loaded[TimeFrameIndex(5)][0]);
    }

    SECTION("Empty LineData") {
        std::map<TimeFrameIndex, std::vector<Line2D>> const input;
        LineData const line_data(input);

        CSVSingleFileLineSaverOptions save_opts;
        save_opts.filename = "empty.csv";
        save_opts.parent_dir = test_dir.string();

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        CSVSingleFileLineLoaderOptions load_opts;
        load_opts.filepath = (test_dir / "empty.csv").string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.empty());
    }

    SECTION("Negative coordinates") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(1)].push_back(Line2D{{-100.5f, -200.3f}, {-0.1f, -0.2f}});

        LineData const line_data(input);

        CSVSingleFileLineSaverOptions save_opts;
        save_opts.filename = "negative.csv";
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 2;

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        CSVSingleFileLineLoaderOptions load_opts;
        load_opts.filepath = (test_dir / "negative.csv").string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 1);
        verifyLineMatch(input[TimeFrameIndex(1)][0], loaded[TimeFrameIndex(1)][0]);
    }

    // Cleanup
    std::filesystem::remove_all(test_dir);
}

TEST_CASE("DM - IO - LineData CSV Unit - Multi file round-trip", "[LineData][CSV][IO][unit]") {

    auto const test_dir = std::filesystem::temp_directory_path() / "line_csv_unit_multi";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    SECTION("Single frame single line") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(42)].push_back(Line2D{{1.0f, 2.0f}, {3.0f, 4.0f}});

        LineData const line_data(input);

        CSVMultiFileLineSaverOptions save_opts;
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 2;
        save_opts.overwrite_existing = true;

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        // Verify file was created
        REQUIRE(std::filesystem::exists(test_dir / "0000042.csv"));

        CSVMultiFileLineLoaderOptions load_opts;
        load_opts.parent_dir = test_dir.string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded.count(TimeFrameIndex(42)) == 1);
        REQUIRE(loaded[TimeFrameIndex(42)].size() == 1);
        verifyLineMatch(input[TimeFrameIndex(42)][0], loaded[TimeFrameIndex(42)][0]);
    }

    SECTION("Multiple frames") {
        // Clear directory for this section
        std::filesystem::remove_all(test_dir);
        std::filesystem::create_directories(test_dir);

        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(0)].push_back(Line2D{{10.0f, 20.0f}, {30.0f, 40.0f}});
        input[TimeFrameIndex(100)].push_back(Line2D{{-5.0f, -10.0f}, {-15.0f, -20.0f}, {-25.0f, -30.0f}});

        LineData const line_data(input);

        CSVMultiFileLineSaverOptions save_opts;
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 1;
        save_opts.overwrite_existing = true;

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        REQUIRE(std::filesystem::exists(test_dir / "0000000.csv"));
        REQUIRE(std::filesystem::exists(test_dir / "0000100.csv"));

        CSVMultiFileLineLoaderOptions load_opts;
        load_opts.parent_dir = test_dir.string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 2);
        verifyLineMatch(input[TimeFrameIndex(0)][0], loaded[TimeFrameIndex(0)][0]);
        verifyLineMatch(input[TimeFrameIndex(100)][0], loaded[TimeFrameIndex(100)][0]);
    }

    SECTION("No-header mode") {
        std::filesystem::remove_all(test_dir);
        std::filesystem::create_directories(test_dir);

        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(7)].push_back(Line2D{{1.5f, 2.5f}, {3.5f, 4.5f}});

        LineData const line_data(input);

        CSVMultiFileLineSaverOptions save_opts;
        save_opts.parent_dir = test_dir.string();
        save_opts.save_header = false;
        save_opts.precision = 1;
        save_opts.overwrite_existing = true;

        bool const ok = save(&line_data, save_opts);
        REQUIRE(ok);

        CSVMultiFileLineLoaderOptions load_opts;
        load_opts.parent_dir = test_dir.string();
        load_opts.has_header = false;

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 1);
        verifyLineMatch(input[TimeFrameIndex(7)][0], loaded[TimeFrameIndex(7)][0]);
    }

    // Cleanup
    std::filesystem::remove_all(test_dir);
}
