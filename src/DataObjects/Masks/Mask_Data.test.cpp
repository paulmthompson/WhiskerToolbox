/**
 * @file Mask_Data.test.cpp
 * @brief MaskData-specific unit tests
 *
 * Shared RaggedTimeSeries behaviour (add/get/clear, entity IDs, observers,
 * range queries, copy/move) is covered by the templated tests in
 *   tests/DataManager/ragged_shared_unit.test.cpp
 *   tests/DataManager/ragged_entity_integration.test.cpp
 *
 * This file only tests behaviour that is unique to MaskData / Mask2D:
 *   - ImageSize get/set
 *   - Construction from (x_vec, y_vec) coordinate style
 *   - Mask2D-specific data verification
 */

#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

TEST_CASE("MaskData - ImageSize get/set", "[mask][imagesize]") {
    MaskData mask_data;

    ImageSize size{640, 480};
    mask_data.setImageSize(size);

    auto retrieved = mask_data.getImageSize();
    REQUIRE(retrieved.width == 640);
    REQUIRE(retrieved.height == 480);
}

TEST_CASE("MaskData - Coordinate-style construction verifies data",
          "[mask][data][coords]") {
    MaskData mask_data;

    std::vector<uint32_t> x1 = {1, 2, 3, 1};
    std::vector<uint32_t> y1 = {1, 1, 2, 2};

    mask_data.addAtTime(TimeFrameIndex(0), Mask2D(x1, y1), NotifyObservers::No);

    auto masks = mask_data.getAtTime(TimeFrameIndex(0));
    REQUIRE(masks.size() == 1);
    REQUIRE(masks[0].size() == 4);
    REQUIRE(masks[0][0].x == 1);
    REQUIRE(masks[0][0].y == 1);
}

TEST_CASE("MaskData - Point-list construction verifies data",
          "[mask][data][pointlist]") {
    MaskData mask_data;

    Mask2D points = {{10, 10}, {11, 10}, {11, 11}, {10, 11}};
    mask_data.addAtTime(TimeFrameIndex(0), points, NotifyObservers::No);

    auto masks = mask_data.getAtTime(TimeFrameIndex(0));
    REQUIRE(masks.size() == 1);
    REQUIRE(masks[0].size() == 4);
    REQUIRE(masks[0][0].x == 10);
}

TEST_CASE("MaskData - Empty mask vectors", "[mask][data][empty]") {
    MaskData mask_data;

    std::vector<uint32_t> empty_x;
    std::vector<uint32_t> empty_y;
    mask_data.addAtTime(TimeFrameIndex(0), Mask2D(empty_x, empty_y), NotifyObservers::No);

    auto masks = mask_data.getAtTime(TimeFrameIndex(0));
    REQUIRE(masks.size() == 1);
    REQUIRE(masks[0].empty());
}
