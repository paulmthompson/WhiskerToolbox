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
#include "Masks/utils/mask_utils.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <set>
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

TEST_CASE("MaskData - changeImageSize matches resize_mask", "[mask][imagesize][scaling]") {
    MaskData mask_data;
    ImageSize const old_size{10, 10};
    ImageSize const new_size{20, 20};
    mask_data.setImageSize(old_size);

    Mask2D mask;
    for (uint32_t x = 2; x <= 4; ++x) {
        for (uint32_t y = 2; y <= 4; ++y) {
            mask.push_back({x, y});
        }
    }
    mask_data.addAtTime(TimeFrameIndex(0), mask, NotifyObservers::No);

    auto const expected = resize_mask(mask, old_size, new_size);

    mask_data.changeImageSize(new_size);

    REQUIRE(mask_data.getImageSize().width == 20);
    REQUIRE(mask_data.getImageSize().height == 20);

    auto const scaled = mask_data.getAtTime(TimeFrameIndex(0));
    REQUIRE(scaled.size() == 1);

    auto to_set = [](Mask2D const & points) {
        std::set<std::pair<uint32_t, uint32_t>> out;
        for (auto const & p: points) {
            out.insert({p.x, p.y});
        }
        return out;
    };

    CHECK(to_set(scaled[0]) == to_set(expected));
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
