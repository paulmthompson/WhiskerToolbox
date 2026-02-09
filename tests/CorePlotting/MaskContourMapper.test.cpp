#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Mappers/MaskContourMapper.hpp"

#include "CoreGeometry/masks.hpp"
#include "Entity/EntityTypes.hpp"
#include "Masks/Mask_Data.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <vector>

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Helpers
// ============================================================================

namespace {

std::shared_ptr<TimeFrame> createLinearTimeFrame(int num_frames) {
    std::vector<int> times;
    times.reserve(static_cast<size_t>(num_frames));
    for (int i = 0; i < num_frames; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

// Create a simple square mask (3x3 filled square)
Mask2D createSquareMask() {
    std::vector<Point2D<uint32_t>> pixels;
    for (uint32_t y = 0; y < 3; ++y) {
        for (uint32_t x = 0; x < 3; ++x) {
            pixels.push_back({x, y});
        }
    }
    return Mask2D{pixels};
}

} // namespace

// ============================================================================
// MaskContourMapper::mapMaskContour Tests
// ============================================================================

TEST_CASE("MaskContourMapper::mapMaskContour - empty mask", "[Mappers][MaskContourMapper]") {
    Mask2D empty_mask;
    auto result = MaskContourMapper::mapMaskContour(empty_mask, EntityId{1});
    REQUIRE(result.vertices().empty());
    REQUIRE(result.entity_id == EntityId{1});
}

TEST_CASE("MaskContourMapper::mapMaskContour - square mask produces contour", "[Mappers][MaskContourMapper]") {
    auto mask = createSquareMask();
    auto result = MaskContourMapper::mapMaskContour(mask, EntityId{42});

    REQUIRE(result.entity_id == EntityId{42});
    // get_mask_outline returns extremal points forming a boundary
    // For a 3x3 square, it should have some outline points
    REQUIRE(!result.vertices().empty());
}

TEST_CASE("MaskContourMapper::mapMaskContour - with scaling", "[Mappers][MaskContourMapper]") {
    auto mask = createSquareMask();
    auto result = MaskContourMapper::mapMaskContour(mask, EntityId{1}, 2.0f, 3.0f, 10.0f, 20.0f);

    REQUIRE(!result.vertices().empty());
    // All vertices should have the scale and offset applied
    for (auto const & v : result.vertices()) {
        // Original coordinates are in [0,2], so scaled x in [10, 14], y in [20, 26]
        REQUIRE(v.x >= 10.0f - 0.01f);
        REQUIRE(v.y >= 20.0f - 0.01f);
    }
}

// ============================================================================
// MaskContourMapper::mapMaskContoursAtTime Tests
// ============================================================================

TEST_CASE("MaskContourMapper::mapMaskContoursAtTime - empty data", "[Mappers][MaskContourMapper]") {
    MaskData masks;
    auto tf = createLinearTimeFrame(10);
    masks.setTimeFrame(tf);

    auto result = MaskContourMapper::mapMaskContoursAtTime(masks, TimeFrameIndex{0});
    REQUIRE(result.empty());
}

TEST_CASE("MaskContourMapper::mapMaskContoursAtTime - single mask at time", "[Mappers][MaskContourMapper]") {
    MaskData masks;
    auto tf = createLinearTimeFrame(10);
    masks.setTimeFrame(tf);

    auto square = createSquareMask();
    std::vector<Mask2D> mask_vec = {square};
    masks.addAtTime(TimeFrameIndex{3}, mask_vec, NotifyObservers::No);

    auto result = MaskContourMapper::mapMaskContoursAtTime(masks, TimeFrameIndex{3});
    REQUIRE(result.size() == 1);
    REQUIRE(!result[0].vertices().empty());
}

TEST_CASE("MaskContourMapper::mapMaskContoursAtTime - multiple masks at time", "[Mappers][MaskContourMapper]") {
    MaskData masks;
    auto tf = createLinearTimeFrame(10);
    masks.setTimeFrame(tf);

    auto square1 = createSquareMask();
    auto square2 = createSquareMask();
    std::vector<Mask2D> mask_vec = {square1, square2};
    masks.addAtTime(TimeFrameIndex{5}, mask_vec, NotifyObservers::No);

    auto result = MaskContourMapper::mapMaskContoursAtTime(masks, TimeFrameIndex{5});
    REQUIRE(result.size() == 2);
}

TEST_CASE("MaskContourMapper::mapMaskContoursAtTime - no data at requested time", "[Mappers][MaskContourMapper]") {
    MaskData masks;
    auto tf = createLinearTimeFrame(10);
    masks.setTimeFrame(tf);

    auto square = createSquareMask();
    std::vector<Mask2D> mask_vec = {square};
    masks.addAtTime(TimeFrameIndex{3}, mask_vec, NotifyObservers::No);

    // Request a different time
    auto result = MaskContourMapper::mapMaskContoursAtTime(masks, TimeFrameIndex{7});
    REQUIRE(result.empty());
}
