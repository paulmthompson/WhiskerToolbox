/**
 * @file mask_csv_rle.test.cpp
 * @brief Unit and integration tests for MaskData CSV RLE encoding/decoding
 *
 * Tests include:
 * 1. RLE encode/decode roundtrip for Mask2D
 * 2. Direct save/load via CSVMaskRLELoaderOptions/CSVMaskRLESaverOptions
 * 3. CSVLoader factory integration for mask data
 * 4. Edge cases: empty masks, single-pixel masks, large runs
 */

#include <catch2/catch_test_macros.hpp>

#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "IO/core/IOTypes.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/formats/CSV/CSVLoader.hpp"
#include "IO/formats/CSV/mask/Mask_Data_CSV.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ranges>
#include <string>
#include <variant>

// ============================================================================
// RLE Encode/Decode Unit Tests
// ============================================================================

TEST_CASE("encode_mask_rle encodes simple horizontal run", "[MaskCSV][RLE]") {
    // 3 consecutive pixels at y=5, x=10..12
    Mask2D mask({{10, 5}, {11, 5}, {12, 5}});

    auto rle = encode_mask_rle(mask);

    // Expect: "5,10,3"
    CHECK(rle == "5,10,3");
}

TEST_CASE("encode_mask_rle encodes multiple runs on different rows", "[MaskCSV][RLE]") {
    Mask2D mask({{0, 0}, {1, 0}, {2, 0},// row 0: run of 3 at x=0
                 {5, 3},
                 {6, 3}});// row 3: run of 2 at x=5

    auto rle = encode_mask_rle(mask);

    CHECK(rle == "0,0,3,3,5,2");
}

TEST_CASE("encode_mask_rle encodes non-contiguous pixels as separate runs", "[MaskCSV][RLE]") {
    // Two separate pixels on the same row
    Mask2D mask({{0, 1}, {5, 1}});

    auto rle = encode_mask_rle(mask);

    // Each pixel becomes its own run of length 1
    CHECK(rle == "1,0,1,1,5,1");
}

TEST_CASE("encode_mask_rle returns empty string for empty mask", "[MaskCSV][RLE]") {
    Mask2D mask;
    auto rle = encode_mask_rle(mask);
    CHECK(rle.empty());
}

TEST_CASE("decode_mask_rle decodes simple run", "[MaskCSV][RLE]") {
    auto mask = decode_mask_rle("5,10,3");

    REQUIRE(mask.size() == 3);
    CHECK(mask[0].x == 10);
    CHECK(mask[0].y == 5);
    CHECK(mask[1].x == 11);
    CHECK(mask[1].y == 5);
    CHECK(mask[2].x == 12);
    CHECK(mask[2].y == 5);
}

TEST_CASE("decode_mask_rle returns empty mask for empty string", "[MaskCSV][RLE]") {
    auto mask = decode_mask_rle("");
    CHECK(mask.empty());
}

TEST_CASE("RLE encode/decode roundtrip preserves mask pixels", "[MaskCSV][RLE]") {
    // Create a mask with various patterns
    Mask2D original({{0, 0}, {1, 0}, {2, 0}, {10, 5}, {11, 5}, {12, 5}, {13, 5}, {100, 200}});

    auto rle = encode_mask_rle(original);
    auto decoded = decode_mask_rle(rle);

    REQUIRE(decoded.size() == original.size());

    // Sort both for comparison (encode sorts internally)
    auto orig_points = original.points();
    auto dec_points = decoded.points();
    auto cmp = [](auto const & a, auto const & b) {
        return (a.y < b.y) || (a.y == b.y && a.x < b.x);
    };
    std::sort(orig_points.begin(), orig_points.end(), cmp);
    std::sort(dec_points.begin(), dec_points.end(), cmp);

    for (size_t i = 0; i < orig_points.size(); ++i) {
        CHECK(dec_points[i].x == orig_points[i].x);
        CHECK(dec_points[i].y == orig_points[i].y);
    }
}

// ============================================================================
// File Save/Load Tests
// ============================================================================

class MaskCSVRLETestFixture {
public:
    MaskCSVRLETestFixture() {
        test_dir = std::filesystem::current_path() / "test_mask_csv_rle_output";
        std::filesystem::create_directories(test_dir);
        csv_filepath = test_dir / "test_mask_rle.csv";
        createTestMaskData();
    }

    ~MaskCSVRLETestFixture() {
        std::filesystem::remove_all(test_dir);
    }

protected:
    void createTestMaskData() {
        original_mask_data = std::make_shared<MaskData>();

        // Frame 0: small rectangular mask (3x2)
        Mask2D mask0({{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}});
        original_mask_data->addAtTime(TimeFrameIndex(0), std::move(mask0), NotifyObservers::No);

        // Frame 5: single pixel mask
        Mask2D mask5({{10, 20}});
        original_mask_data->addAtTime(TimeFrameIndex(5), std::move(mask5), NotifyObservers::No);

        // Frame 10: larger L-shaped mask
        Mask2D mask10({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {0, 1}, {0, 2}});
        original_mask_data->addAtTime(TimeFrameIndex(10), std::move(mask10), NotifyObservers::No);
    }

    std::filesystem::path test_dir;
    std::filesystem::path csv_filepath;
    std::shared_ptr<MaskData> original_mask_data;
};

TEST_CASE_METHOD(MaskCSVRLETestFixture,
                 "Save and load MaskData via CSV RLE roundtrip",
                 "[MaskCSV][RLE][File]") {
    // Save
    CSVMaskRLESaverOptions save_opts;
    save_opts.parent_dir = test_dir.string();
    save_opts.filename = "test_mask_rle.csv";
    save(original_mask_data.get(), save_opts);

    REQUIRE(std::filesystem::exists(csv_filepath));

    // Load
    CSVMaskRLELoaderOptions load_opts;
    load_opts.filepath = csv_filepath.string();

    auto loaded_map = load(load_opts);

    // Verify frame count
    REQUIRE(loaded_map.size() == 3);
    CHECK(loaded_map.count(TimeFrameIndex(0)) == 1);
    CHECK(loaded_map.count(TimeFrameIndex(5)) == 1);
    CHECK(loaded_map.count(TimeFrameIndex(10)) == 1);

    // Verify pixel count at frame 0
    auto const & masks_at_0 = loaded_map[TimeFrameIndex(0)];
    REQUIRE(masks_at_0.size() == 1);
    CHECK(masks_at_0[0].size() == 6);

    // Verify pixel count at frame 5
    auto const & masks_at_5 = loaded_map[TimeFrameIndex(5)];
    REQUIRE(masks_at_5.size() == 1);
    CHECK(masks_at_5[0].size() == 1);

    // Verify pixel count at frame 10
    auto const & masks_at_10 = loaded_map[TimeFrameIndex(10)];
    REQUIRE(masks_at_10.size() == 1);
    CHECK(masks_at_10[0].size() == 6);
}

// ============================================================================
// CSVLoader Factory Integration Tests
// ============================================================================

TEST_CASE_METHOD(MaskCSVRLETestFixture,
                 "CSVLoader supports Mask data type",
                 "[MaskCSV][CSVLoader]") {
    CSVLoader loader;
    CHECK(loader.supportsFormat("csv", IODataType::Mask));
    CHECK_FALSE(loader.supportsFormat("dlc_csv", IODataType::Mask));
}

TEST_CASE_METHOD(MaskCSVRLETestFixture,
                 "CSVLoader load and save Mask data via factory",
                 "[MaskCSV][CSVLoader][Integration]") {
    CSVLoader loader;

    // Save via factory
    nlohmann::json save_config;
    auto save_result = loader.save(csv_filepath.string(), IODataType::Mask, save_config,
                                   original_mask_data.get());
    REQUIRE(save_result.success);
    REQUIRE(std::filesystem::exists(csv_filepath));

    // Load via factory
    nlohmann::json load_config;
    auto load_result = loader.load(csv_filepath.string(), IODataType::Mask, load_config);
    REQUIRE(load_result.success);
    REQUIRE(std::holds_alternative<std::shared_ptr<MaskData>>(load_result.data));

    auto loaded_mask = std::get<std::shared_ptr<MaskData>>(load_result.data);
    REQUIRE(loaded_mask != nullptr);

    // The original had 3 masks across 3 frames
    // Verify by checking a known frame
    auto masks_at_0 = loaded_mask->getAtTime(TimeFrameIndex(0));
    REQUIRE(std::ranges::distance(masks_at_0) == 1);
    auto const & first_mask = *masks_at_0.begin();
    CHECK(first_mask.size() == 6);
}
