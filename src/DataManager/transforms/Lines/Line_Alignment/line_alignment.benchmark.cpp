#include "transforms/Lines/Line_Alignment/line_alignment.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "../mocks/mock_media_data.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <vector>
#include <utility>

/**
 * @brief Generate a random test image with bright features
 * @param image_size The size of the image to generate
 * @param num_features Number of bright features to add
 * @param feature_intensity The intensity of the bright features
 * @return A vector containing the generated image data
 */
std::vector<uint8_t> generateTestImage(ImageSize const & image_size,
                                       int num_features = 5,
                                       uint8_t feature_intensity = 255) {
    std::vector<uint8_t> image_data(image_size.width * image_size.height, 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> x_dist(10, image_size.width - 10);
    std::uniform_int_distribution<> y_dist(10, image_size.height - 10);
    std::uniform_int_distribution<> length_dist(20, 40);
    std::uniform_int_distribution<> orientation_dist(0, 3);// 0=horizontal, 1=vertical, 2=diagonal \, 3=diagonal /

    for (int f = 0; f < num_features; ++f) {
        int center_x = x_dist(gen);
        int center_y = y_dist(gen);
        int length = length_dist(gen);
        int orientation = orientation_dist(gen);

        // Create a bright line feature
        for (int i = -length / 2; i <= length / 2; ++i) {
            int x = center_x;
            int y = center_y;

            switch (orientation) {
                case 0:// Horizontal
                    x += i;
                    break;
                case 1:// Vertical
                    y += i;
                    break;
                case 2:// Diagonal \
                    x += i;
                    y += i;
                    break;
                case 3:// Diagonal /
                    x += i;
                    y -= i;
                    break;
            }

            if (x >= 0 && x < image_size.width && y >= 0 && y < image_size.height) {
                image_data[y * image_size.width + x] = feature_intensity;
            }
        }
    }

    return image_data;
}

/**
 * @brief Generate a random test line
 * @param image_size The size of the image
 * @param num_vertices Number of vertices in the line
 * @return A Line2D containing the generated line
 */
Line2D generateTestLine(ImageSize const & image_size, int num_vertices = 5) {
    Line2D line;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> x_dist(10, image_size.width - 10);
    std::uniform_int_distribution<> y_dist(10, image_size.height - 10);

    for (int i = 0; i < num_vertices; ++i) {
        float x = static_cast<float>(x_dist(gen));
        float y = static_cast<float>(y_dist(gen));
        line.push_back(Point2D<float>{x, y});
    }

    return line;
}

/**
 * @brief Create a single multi-frame dataset (8-bit) with multiple images and per-frame lines
 * @param num_images Number of frames/images to create within one media instance
 * @param image_size Size of each image
 * @param lines_per_image Number of lines per frame
 * @return A pair (media_data, line_data) sharing a common timeframe
 */
std::pair<std::shared_ptr<MockMediaData>, std::shared_ptr<LineData>>
createMultiFrameDataset8(int num_images, ImageSize const & image_size, int lines_per_image = 3) {
    auto time_frame = std::make_shared<TimeFrame>();

    auto media_data = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit8);
    media_data->setTimeFrame(time_frame);

    auto line_data = std::make_shared<LineData>();
    line_data->setImageSize(image_size);
    line_data->setTimeFrame(time_frame);

    for (int i = 0; i < num_images; ++i) {
        // Append a frame to the single media instance
        auto image_data = generateTestImage(image_size, 3 + (i % 3));
        media_data->addImage8(image_data, image_size);

        // Add lines for this frame at time index i
        for (int j = 0; j < lines_per_image; ++j) {
            auto test_line = generateTestLine(image_size, 4 + (j % 3));// 4-6 vertices per line
            line_data->addAtTime(TimeFrameIndex(i), test_line, false);
        }
    }

    return {media_data, line_data};
}

/**
 * @brief Create a single multi-frame dataset (32-bit float) with multiple images and per-frame lines
 */
std::pair<std::shared_ptr<MockMediaData>, std::shared_ptr<LineData>>
createMultiFrameDataset32(int num_images, ImageSize const & image_size, int lines_per_image = 3) {
    auto time_frame = std::make_shared<TimeFrame>();

    auto media_data = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit32);
    media_data->setTimeFrame(time_frame);

    auto line_data = std::make_shared<LineData>();
    line_data->setImageSize(image_size);
    line_data->setTimeFrame(time_frame);

    for (int i = 0; i < num_images; ++i) {
        // Create an 8-bit synthetic image and convert to float [0,1]
        auto image_data_8 = generateTestImage(image_size, 3 + (i % 3));
        std::vector<float> image_data_32;
        image_data_32.reserve(image_data_8.size());
        for (uint8_t pixel: image_data_8) {
            image_data_32.push_back(static_cast<float>(pixel) / 255.0f);
        }
        media_data->addImage32(image_data_32, image_size);

        for (int j = 0; j < lines_per_image; ++j) {
            auto test_line = generateTestLine(image_size, 4 + (j % 3));
            line_data->addAtTime(TimeFrameIndex(i), test_line, false);
        }
    }

    return {media_data, line_data};
}

TEST_CASE("Benchmark: Line Alignment - Single Image Processing", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    // Create single-frame dataset
    auto [media_data, line_data] = createMultiFrameDataset8(1, image_size, 1);
    // Extract pointers before OpenMP region (structured bindings can't be captured in OpenMP)
    auto* line_data_ptr = line_data.get();
    auto* media_data_ptr = media_data.get();

    BENCHMARK("Single Image Line Alignment - 8-bit") {
        return line_alignment(
                line_data_ptr,
                media_data_ptr,
                width,
                perpendicular_range,
                false,// use_processed_data
                FWHMApproach::PEAK_WIDTH_HALF_MAX,
                LineAlignmentOutputMode::ALIGNED_VERTICES);
    };
}

TEST_CASE("Benchmark: Line Alignment - Multiple Images", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int num_images = 10;
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    // Create multi-frame dataset
    auto [media_data, line_data] = createMultiFrameDataset8(num_images, image_size, 3);
    auto* line_data_ptr = line_data.get();
    auto* media_data_ptr = media_data.get();

    BENCHMARK("10 Images Line Alignment - 8-bit") {
        return line_alignment(
                line_data_ptr,
                media_data_ptr,
                width,
                perpendicular_range,
                false,// use_processed_data
                FWHMApproach::PEAK_WIDTH_HALF_MAX,
                LineAlignmentOutputMode::ALIGNED_VERTICES);
    };
}

TEST_CASE("Benchmark: Line Alignment - Different Image Sizes", "[line][alignment][benchmark]") {
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;
    constexpr int num_images = 10;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    SECTION("50x50 Images") {
        constexpr ImageSize image_size{50, 50};
        auto [media_data, line_data] = createMultiFrameDataset8(num_images, image_size, 2);
        auto* line_data_ptr = line_data.get();
        auto* media_data_ptr = media_data.get();

        BENCHMARK("10 Images 50x50 Line Alignment") {
            return line_alignment(
                    line_data_ptr,
                    media_data_ptr,
                    width,
                    perpendicular_range,
                    false,
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::ALIGNED_VERTICES);
        };
    }

    SECTION("200x200 Images") {
        constexpr ImageSize image_size{200, 200};
        auto [media_data, line_data] = createMultiFrameDataset8(num_images, image_size, 2);
        auto* line_data_ptr = line_data.get();
        auto* media_data_ptr = media_data.get();

        BENCHMARK("10 Images 200x200 Line Alignment") {
            return line_alignment(
                    line_data_ptr,
                    media_data_ptr,
                    width,
                    perpendicular_range,
                    false,
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::ALIGNED_VERTICES);
        };
    }
}

TEST_CASE("Benchmark: Line Alignment - Different Parameters", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int num_images = 10;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    auto [media_data_small, line_data_small] = createMultiFrameDataset8(num_images, image_size, 2);
    auto* line_data_small_ptr = line_data_small.get();
    auto* media_data_small_ptr = media_data_small.get();

    SECTION("Small Width and Range") {
        constexpr int width = 10;
        constexpr int perpendicular_range = 25;

        BENCHMARK("Small Parameters (width=10, range=25)") {
            return line_alignment(
                    line_data_small_ptr,
                    media_data_small_ptr,
                    width,
                    perpendicular_range,
                    false,
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::ALIGNED_VERTICES);
        };
    }

    SECTION("Large Width and Range") {
        constexpr int width = 40;
        constexpr int perpendicular_range = 100;
        auto [media_data_large, line_data_large] = createMultiFrameDataset8(num_images, image_size, 2);
        auto* line_data_large_ptr = line_data_large.get();
        auto* media_data_large_ptr = media_data_large.get();

        BENCHMARK("Large Parameters (width=40, range=100)") {
            return line_alignment(
                    line_data_large_ptr,
                    media_data_large_ptr,
                    width,
                    perpendicular_range,
                    false,
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::ALIGNED_VERTICES);
        };
    }
}

TEST_CASE("Benchmark: Line Alignment - FWHM Profile Extents Mode", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int num_images = 10;
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    auto [media_data_multi, line_data_multi] = createMultiFrameDataset8(num_images, image_size, 2);
    auto* line_data_multi_ptr = line_data_multi.get();
    auto* media_data_multi_ptr = media_data_multi.get();

    BENCHMARK("FWHM Profile Extents Mode") {
        return line_alignment(
                line_data_multi_ptr,
                media_data_multi_ptr,
                width,
                perpendicular_range,
                false,
                FWHMApproach::PEAK_WIDTH_HALF_MAX,
                LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS);
    };
}

TEST_CASE("Benchmark: Line Alignment - 32-bit vs 8-bit", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int num_images = 10;
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    // Create 8-bit and 32-bit multi-frame datasets
    auto [media_data_8bit, line_data_8bit] = createMultiFrameDataset8(num_images, image_size, 2);
    auto* line_data_8bit_ptr = line_data_8bit.get();
    auto* media_data_8bit_ptr = media_data_8bit.get();
    
    auto [media_data_32bit, line_data_32bit] = createMultiFrameDataset32(num_images, image_size, 2);
    auto* line_data_32bit_ptr = line_data_32bit.get();
    auto* media_data_32bit_ptr = media_data_32bit.get();

    SECTION("32-bit Float Data") {
        BENCHMARK("32-bit Float Data") {
            return line_alignment(
                    line_data_32bit_ptr,
                    media_data_32bit_ptr,
                    width,
                    perpendicular_range,
                    false,
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::ALIGNED_VERTICES);
        };
    }

    SECTION("8-bit Data") {
        BENCHMARK("8-bit Data") {
            return line_alignment(
                    line_data_8bit_ptr,
                    media_data_8bit_ptr,
                    width,
                    perpendicular_range,
                    false,
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::ALIGNED_VERTICES);
        };
    }
}
