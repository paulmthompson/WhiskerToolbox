#include "transforms/Lines/Line_Alignment/line_alignment.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/Lines/Line_Alignment/mock_media_data.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <vector>

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
 * @brief Create a test dataset with multiple images and lines
 * @param num_images Number of images to create
 * @param image_size Size of each image
 * @param lines_per_image Number of lines per image
 * @return A vector of pairs containing (media_data, line_data)
 */
std::vector<std::pair<std::shared_ptr<MockMediaData>, std::shared_ptr<LineData>>>
createTestDataset(int num_images, ImageSize const & image_size, int lines_per_image = 3) {
    std::vector<std::pair<std::shared_ptr<MockMediaData>, std::shared_ptr<LineData>>> dataset;

    auto time_frame = std::make_shared<TimeFrame>();

    for (int i = 0; i < num_images; ++i) {
        // Create media data with random image
        auto media_data = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit8);
        auto image_data = generateTestImage(image_size, 3 + (i % 3));// 3-5 features per image
        media_data->addImage8(image_data, image_size);
        media_data->setTimeFrame(time_frame);

        // Create line data with random lines
        auto line_data = std::make_shared<LineData>();
        line_data->setImageSize(image_size);
        line_data->setTimeFrame(time_frame);

        for (int j = 0; j < lines_per_image; ++j) {
            auto test_line = generateTestLine(image_size, 4 + (j % 3));// 4-6 vertices per line
            line_data->addAtTime(TimeFrameIndex(0), test_line, false);
        }

        dataset.emplace_back(media_data, line_data);
    }

    return dataset;
}

TEST_CASE("Benchmark: Line Alignment - Single Image Processing", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    // Create test data
    auto media_data = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit8);
    auto image_data = generateTestImage(image_size, 5);
    media_data->addImage8(image_data, image_size);

    auto line_data = std::make_shared<LineData>();
    line_data->setImageSize(image_size);
    auto test_line = generateTestLine(image_size, 5);
    line_data->addAtTime(TimeFrameIndex(0), test_line, false);

    BENCHMARK("Single Image Line Alignment - 8-bit") {
        return line_alignment(
                line_data.get(),
                media_data.get(),
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

    // Create test dataset
    auto dataset = createTestDataset(num_images, image_size, 3);

    BENCHMARK("10 Images Line Alignment - 8-bit") {
        std::vector<std::shared_ptr<LineData>> results;
        results.reserve(num_images);

        for (auto const & [media_data, line_data]: dataset) {
            auto result = line_alignment(
                    line_data.get(),
                    media_data.get(),
                    width,
                    perpendicular_range,
                    false,// use_processed_data
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::ALIGNED_VERTICES);
            results.push_back(result);
        }

        return results;
    };
}

TEST_CASE("Benchmark: Line Alignment - Different Image Sizes", "[line][alignment][benchmark]") {
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;
    constexpr int num_images = 10;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    SECTION("50x50 Images") {
        constexpr ImageSize image_size{50, 50};
        auto dataset = createTestDataset(num_images, image_size, 2);

        BENCHMARK("10 Images 50x50 Line Alignment") {
            std::vector<std::shared_ptr<LineData>> results;
            results.reserve(num_images);

            for (auto const & [media_data, line_data]: dataset) {
                auto result = line_alignment(
                        line_data.get(),
                        media_data.get(),
                        width,
                        perpendicular_range,
                        false,
                        FWHMApproach::PEAK_WIDTH_HALF_MAX,
                        LineAlignmentOutputMode::ALIGNED_VERTICES);
                results.push_back(result);
            }

            return results;
        };
    }

    SECTION("200x200 Images") {
        constexpr ImageSize image_size{200, 200};
        auto dataset = createTestDataset(num_images, image_size, 2);

        BENCHMARK("10 Images 200x200 Line Alignment") {
            std::vector<std::shared_ptr<LineData>> results;
            results.reserve(num_images);

            for (auto const & [media_data, line_data]: dataset) {
                auto result = line_alignment(
                        line_data.get(),
                        media_data.get(),
                        width,
                        perpendicular_range,
                        false,
                        FWHMApproach::PEAK_WIDTH_HALF_MAX,
                        LineAlignmentOutputMode::ALIGNED_VERTICES);
                results.push_back(result);
            }

            return results;
        };
    }
}

TEST_CASE("Benchmark: Line Alignment - Different Parameters", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int num_images = 10;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    auto dataset = createTestDataset(num_images, image_size, 2);

    SECTION("Small Width and Range") {
        constexpr int width = 10;
        constexpr int perpendicular_range = 25;

        BENCHMARK("Small Parameters (width=10, range=25)") {
            std::vector<std::shared_ptr<LineData>> results;
            results.reserve(num_images);

            for (auto const & [media_data, line_data]: dataset) {
                auto result = line_alignment(
                        line_data.get(),
                        media_data.get(),
                        width,
                        perpendicular_range,
                        false,
                        FWHMApproach::PEAK_WIDTH_HALF_MAX,
                        LineAlignmentOutputMode::ALIGNED_VERTICES);
                results.push_back(result);
            }

            return results;
        };
    }

    SECTION("Large Width and Range") {
        constexpr int width = 40;
        constexpr int perpendicular_range = 100;

        BENCHMARK("Large Parameters (width=40, range=100)") {
            std::vector<std::shared_ptr<LineData>> results;
            results.reserve(num_images);

            for (auto const & [media_data, line_data]: dataset) {
                auto result = line_alignment(
                        line_data.get(),
                        media_data.get(),
                        width,
                        perpendicular_range,
                        false,
                        FWHMApproach::PEAK_WIDTH_HALF_MAX,
                        LineAlignmentOutputMode::ALIGNED_VERTICES);
                results.push_back(result);
            }

            return results;
        };
    }
}

TEST_CASE("Benchmark: Line Alignment - FWHM Profile Extents Mode", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int num_images = 10;
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    auto dataset = createTestDataset(num_images, image_size, 2);

    BENCHMARK("FWHM Profile Extents Mode") {
        std::vector<std::shared_ptr<LineData>> results;
        results.reserve(num_images);

        for (auto const & [media_data, line_data]: dataset) {
            auto result = line_alignment(
                    line_data.get(),
                    media_data.get(),
                    width,
                    perpendicular_range,
                    false,
                    FWHMApproach::PEAK_WIDTH_HALF_MAX,
                    LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS);
            results.push_back(result);
        }

        return results;
    };
}

TEST_CASE("Benchmark: Line Alignment - 32-bit vs 8-bit", "[line][alignment][benchmark]") {
    constexpr ImageSize image_size{100, 100};
    constexpr int num_images = 10;
    constexpr int width = 20;
    constexpr int perpendicular_range = 50;

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    // Create 8-bit dataset
    auto dataset_8bit = createTestDataset(num_images, image_size, 2);

    // Create 32-bit dataset
    std::vector<std::pair<std::shared_ptr<MockMediaData>, std::shared_ptr<LineData>>> dataset_32bit;
    auto time_frame = std::make_shared<TimeFrame>();

    for (int i = 0; i < num_images; ++i) {
        // Create media data with 32-bit image
        auto media_data = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit32);
        auto image_data_8bit = generateTestImage(image_size, 3 + (i % 3));

        // Convert to 32-bit float
        std::vector<float> image_data_32bit;
        image_data_32bit.reserve(image_data_8bit.size());
        for (uint8_t pixel: image_data_8bit) {
            image_data_32bit.push_back(static_cast<float>(pixel) / 255.0f);
        }

        media_data->addImage32(image_data_32bit, image_size);
        media_data->setTimeFrame(time_frame);

        // Create line data (same as 8-bit version)
        auto line_data = std::make_shared<LineData>();
        line_data->setImageSize(image_size);
        line_data->setTimeFrame(time_frame);

        for (int j = 0; j < 2; ++j) {
            auto test_line = generateTestLine(image_size, 4 + (j % 3));
            line_data->addAtTime(TimeFrameIndex(0), test_line, false);
        }

        dataset_32bit.emplace_back(media_data, line_data);
    }

    SECTION("8-bit Data") {
        BENCHMARK("32-bit Float Data") {
            std::vector<std::shared_ptr<LineData>> results;
            results.reserve(num_images);

            for (auto const & [media_data, line_data]: dataset_32bit) {
                auto result = line_alignment(
                        line_data.get(),
                        media_data.get(),
                        width,
                        perpendicular_range,
                        false,
                        FWHMApproach::PEAK_WIDTH_HALF_MAX,
                        LineAlignmentOutputMode::ALIGNED_VERTICES);
                results.push_back(result);
            }

            return results;
        };
    }

    SECTION("8-bit Data") {
        BENCHMARK("8-bit Data") {
            std::vector<std::shared_ptr<LineData>> results;
            results.reserve(num_images);

            for (auto const & [media_data, line_data]: dataset_8bit) {
                auto result = line_alignment(
                        line_data.get(),
                        media_data.get(),
                        width,
                        perpendicular_range,
                        false,
                        FWHMApproach::PEAK_WIDTH_HALF_MAX,
                        LineAlignmentOutputMode::ALIGNED_VERTICES);
                results.push_back(result);
            }

            return results;
        };
    }
}
