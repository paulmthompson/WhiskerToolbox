/**
 * @file line_binary_unit.test.cpp
 * @brief Deterministic round-trip unit tests for CapnProto binary LineData save/load
 *
 * Tests that saving LineData to CapnProto binary format and loading it back produces
 * identical data. Binary format is lossless so exact comparison is used.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "IO/formats/CapnProto/linedata/Line_Data_Binary.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

/// Helper to verify two Line2D objects match exactly (binary format is lossless)
void verifyLineMatch(Line2D const & expected, Line2D const & actual) {
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(actual[i].x == expected[i].x);
        REQUIRE(actual[i].y == expected[i].y);
    }
}

}// namespace

TEST_CASE("DM - IO - LineData Binary Unit - Round-trip", "[LineData][Binary][IO][unit]") {

    auto const test_dir = std::filesystem::temp_directory_path() / "line_binary_unit";
    std::filesystem::create_directories(test_dir);

    SECTION("Single line single frame") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(5)].push_back(Line2D{{10.5f, 20.3f}, {30.1f, 40.7f}});

        LineData const line_data(input);

        BinaryLineSaverOptions save_opts;
        save_opts.filename = "single_line.capnp";
        save_opts.parent_dir = test_dir.string();

        bool const ok = save(line_data, save_opts);
        REQUIRE(ok);

        BinaryLineLoaderOptions load_opts;
        load_opts.file_path = (test_dir / "single_line.capnp").string();

        auto loaded = ::load(load_opts);
        REQUIRE(loaded != nullptr);

        auto const times = loaded->getTimesWithData();
        std::vector<TimeFrameIndex> times_vec(times.begin(), times.end());
        REQUIRE(times_vec.size() == 1);
        REQUIRE(times_vec[0] == TimeFrameIndex(5));

        auto const & loaded_lines = loaded->getAtTime(TimeFrameIndex(5));
        REQUIRE(loaded_lines.size() == 1);
        verifyLineMatch(input[TimeFrameIndex(5)][0], loaded_lines[0]);
    }

    SECTION("Multiple lines per frame") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        Line2D const line1 = {{1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f}};
        Line2D const line2 = {{100.0f, 200.0f}, {300.0f, 400.0f}};
        input[TimeFrameIndex(0)].push_back(line1);
        input[TimeFrameIndex(0)].push_back(line2);

        LineData const line_data(input);

        BinaryLineSaverOptions save_opts;
        save_opts.filename = "multi_lines.capnp";
        save_opts.parent_dir = test_dir.string();

        bool const ok = save(line_data, save_opts);
        REQUIRE(ok);

        BinaryLineLoaderOptions load_opts;
        load_opts.file_path = (test_dir / "multi_lines.capnp").string();

        auto loaded = ::load(load_opts);
        REQUIRE(loaded != nullptr);

        auto const & loaded_lines = loaded->getAtTime(TimeFrameIndex(0));
        REQUIRE(loaded_lines.size() == 2);
        verifyLineMatch(line1, loaded_lines[0]);
        verifyLineMatch(line2, loaded_lines[1]);
    }

    SECTION("Multiple frames") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(0)].push_back(Line2D{{10.0f, 20.0f}});
        input[TimeFrameIndex(100)].push_back(Line2D{{30.0f, 40.0f}, {50.0f, 60.0f}});
        input[TimeFrameIndex(500)].push_back(Line2D{{70.0f, 80.0f}});

        LineData const line_data(input);

        BinaryLineSaverOptions save_opts;
        save_opts.filename = "multi_frames.capnp";
        save_opts.parent_dir = test_dir.string();

        bool const ok = save(line_data, save_opts);
        REQUIRE(ok);

        BinaryLineLoaderOptions load_opts;
        load_opts.file_path = (test_dir / "multi_frames.capnp").string();

        auto loaded = ::load(load_opts);
        REQUIRE(loaded != nullptr);

        auto const times = loaded->getTimesWithData();
        std::vector<TimeFrameIndex> times_vec(times.begin(), times.end());
        std::sort(times_vec.begin(), times_vec.end());

        REQUIRE(times_vec.size() == 3);
        REQUIRE(times_vec[0] == TimeFrameIndex(0));
        REQUIRE(times_vec[1] == TimeFrameIndex(100));
        REQUIRE(times_vec[2] == TimeFrameIndex(500));

        for (auto const & [frame, orig_lines]: input) {
            auto const & loaded_lines = loaded->getAtTime(frame);
            REQUIRE(loaded_lines.size() == orig_lines.size());
            for (size_t i = 0; i < orig_lines.size(); ++i) {
                verifyLineMatch(orig_lines[i], loaded_lines[i]);
            }
        }
    }

    SECTION("Negative coordinates preserved") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(0)].push_back(
                Line2D{{-100.5f, -200.3f}, {-0.001f, 0.001f}, {999.999f, -999.999f}});

        LineData const line_data(input);

        BinaryLineSaverOptions save_opts;
        save_opts.filename = "negative_coords.capnp";
        save_opts.parent_dir = test_dir.string();

        bool const ok = save(line_data, save_opts);
        REQUIRE(ok);

        BinaryLineLoaderOptions load_opts;
        load_opts.file_path = (test_dir / "negative_coords.capnp").string();

        auto loaded = ::load(load_opts);
        REQUIRE(loaded != nullptr);

        auto const & loaded_lines = loaded->getAtTime(TimeFrameIndex(0));
        REQUIRE(loaded_lines.size() == 1);
        verifyLineMatch(input[TimeFrameIndex(0)][0], loaded_lines[0]);
    }

    SECTION("Image size preserved") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(0)].push_back(Line2D{{10.0f, 20.0f}});

        LineData line_data(input);
        line_data.setImageSize(ImageSize{1920, 1080});

        BinaryLineSaverOptions save_opts;
        save_opts.filename = "image_size.capnp";
        save_opts.parent_dir = test_dir.string();

        bool const ok = save(line_data, save_opts);
        REQUIRE(ok);

        BinaryLineLoaderOptions load_opts;
        load_opts.file_path = (test_dir / "image_size.capnp").string();

        auto loaded = ::load(load_opts);
        REQUIRE(loaded != nullptr);

        auto const img_size = loaded->getImageSize();
        REQUIRE(img_size.width == 1920);
        REQUIRE(img_size.height == 1080);
    }

    SECTION("Save returns true on success") {
        std::map<TimeFrameIndex, std::vector<Line2D>> input;
        input[TimeFrameIndex(0)].push_back(Line2D{{1.0f, 2.0f}});

        LineData const line_data(input);

        BinaryLineSaverOptions save_opts;
        save_opts.filename = "success_check.capnp";
        save_opts.parent_dir = test_dir.string();

        bool const ok = save(line_data, save_opts);
        REQUIRE(ok);
        REQUIRE(std::filesystem::exists(test_dir / "success_check.capnp"));
        REQUIRE(std::filesystem::file_size(test_dir / "success_check.capnp") > 0);
    }

    // Cleanup
    std::filesystem::remove_all(test_dir);
}
