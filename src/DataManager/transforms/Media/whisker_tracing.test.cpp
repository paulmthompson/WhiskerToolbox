#include "transforms/Media/whisker_tracing.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/Lines/Line_Alignment/mock_media_data.hpp"
#include "whiskertracker.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdint>
#include <memory>
#include <vector>

TEST_CASE("Data Transform: Whisker Tracing - Mask-based seeding and tracing", "[whisker][tracing][mask][transform]") {

    auto tracker = std::make_shared<whisker::WhiskerTracker>();

    SECTION("Mask size matches media size") {
        // Media: 640x480 white image with dark whiskers (black lines)
        ImageSize media_size{640, 480};
        std::vector<uint8_t> img(static_cast<size_t>(media_size.width * media_size.height), 255);

        // Draw two thin black vertical lines to create structure in the image
        for (int y = 0; y < media_size.height; ++y) {
            img[static_cast<size_t>(y) * static_cast<size_t>(media_size.width) + 120] = 0;
            img[static_cast<size_t>(y) * static_cast<size_t>(media_size.width) + 300] = 0;
        }

        // Media data
        auto media = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit8);
        media->addImage8(img, media_size);
        media->setTimeFrame(std::make_shared<TimeFrame>());

        // Mask: same size as media, seed pixels along the bright column to encourage tracing
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(media_size);
        mask_data->setTimeFrame(std::make_shared<TimeFrame>());
        std::vector<Point2D<uint32_t>> mask_pts;
        for (int y = 50; y < 430; y += 5) {
            mask_pts.push_back(Point2D<uint32_t>{static_cast<uint32_t>(120), static_cast<uint32_t>(y)});
        }
        mask_data->addAtTime(TimeFrameIndex(0), mask_pts, false);

        // Parameters
        WhiskerTracingParameters params;
        params.use_processed_data = false;
        params.clip_length = 0;
        params.whisker_length_threshold = 10.0f;
        params.batch_size = 1;
        params.use_parallel_processing = false;
        params.use_mask_data = true;
        params.mask_data = mask_data;

        // Pre-initialize tracker to avoid expensive setup in tests
        tracker->setWhiskerLengthThreshold(params.whisker_length_threshold);
        params.tracker = tracker;

        // Execute
        WhiskerTracingOperation op;
        DataTypeVariant input = media;
        auto result_variant = op.execute(input, &params);

        auto result = std::get_if<std::shared_ptr<LineData>>(&result_variant);
        REQUIRE(result != nullptr);
        REQUIRE(*result != nullptr);

        // We expect at least one traced whisker at time 0
        auto const & lines = (*result)->getAtTime(TimeFrameIndex(0));
        REQUIRE(lines.size() >= 1);

        // Basic sanity: traced line has multiple points
        REQUIRE(lines[0].size() >= 2);
    }

    SECTION("Mask size differs from media size - nearest neighbor scaling applied") {
        // Media: 640x480 white image
        ImageSize media_size{640, 480};
        std::vector<uint8_t> img(static_cast<size_t>(media_size.width * media_size.height), 255);

        // Add a dark diagonal to create varied gradients
        for (int x = 50; x < 590; ++x) {
            int y = 100 + (x - 50) / 4; // gentle slope
            if (y >= 0 && y < media_size.height) {
                img[static_cast<size_t>(y) * static_cast<size_t>(media_size.width) + x] = 0;
            }
        }

        // Media data
        auto media = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit8);
        media->addImage8(img, media_size);
        media->setTimeFrame(std::make_shared<TimeFrame>());

        // Mask: 256x256 grid, a vertical stripe of seeds near column 60 (of 256)
        ImageSize mask_size{256, 256};
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(mask_size);
        mask_data->setTimeFrame(std::make_shared<TimeFrame>());

        std::vector<Point2D<uint32_t>> mask_pts_small;
        for (int y = 10; y < 246; y += 4) {
            mask_pts_small.push_back(Point2D<uint32_t>{static_cast<uint32_t>(60), static_cast<uint32_t>(y)});
        }
        mask_data->addAtTime(TimeFrameIndex(0), mask_pts_small, false);

        // Parameters use the mismatched-size mask
        WhiskerTracingParameters params;
        params.use_processed_data = false;
        params.clip_length = 0;
        params.whisker_length_threshold = 10.0f;
        params.batch_size = 1;
        params.use_parallel_processing = false;
        params.use_mask_data = true;
        params.mask_data = mask_data;

        // Reuse an initialized tracker
        tracker->setWhiskerLengthThreshold(params.whisker_length_threshold);
        params.tracker = tracker;

        // Execute
        WhiskerTracingOperation op;
        DataTypeVariant input = media;
        auto result_variant = op.execute(input, &params);

        auto result = std::get_if<std::shared_ptr<LineData>>(&result_variant);
        REQUIRE(result != nullptr);
        REQUIRE(*result != nullptr);

        // There should be traced whiskers due to scaled mask seeds
        auto const & lines = (*result)->getAtTime(TimeFrameIndex(0));
        REQUIRE(lines.size() >= 1);

        // Verify image size propagated
        REQUIRE((*result)->getImageSize().width == media_size.width);
        REQUIRE((*result)->getImageSize().height == media_size.height);
    }
}


