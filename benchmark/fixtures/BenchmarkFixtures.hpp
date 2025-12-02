/**
 * @file BenchmarkFixtures.hpp
 * @brief Reusable fixtures for generating test data for benchmarks
 * 
 * This file provides fixtures that generate realistic test data for benchmarking
 * various transforms and algorithms. The fixtures are designed to be:
 * - Deterministic (same seed = same data)
 * - Configurable (size, complexity, etc.)
 * - Representative of real-world data
 */

#ifndef WHISKERTOOLBOX_BENCHMARK_FIXTURES_HPP
#define WHISKERTOOLBOX_BENCHMARK_FIXTURES_HPP

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cstdint>
#include <memory>
#include <random>
#include <set>
#include <vector>

namespace WhiskerToolbox::Benchmark {

// ============================================================================
// Configuration Structures
// ============================================================================

/**
 * @brief Configuration for MaskData generation
 */
struct MaskDataConfig {
    size_t num_frames = 100;       // Number of time frames
    size_t masks_per_frame_min = 1;// Minimum masks per frame
    size_t masks_per_frame_max = 5;// Maximum masks per frame
    size_t mask_size_min = 10;     // Minimum pixels per mask
    size_t mask_size_max = 1000;   // Maximum pixels per mask
    uint32_t image_width = 640;    // Image width
    uint32_t image_height = 480;   // Image height
    int time_start = 0;            // Start time value
    int time_step = 1;             // Time step between frames
    uint32_t random_seed = 42;     // Random seed for reproducibility

    // Mask shape parameters
    bool use_blob_masks = true;   // Generate blob-like masks
    float blob_compactness = 0.7f;// How compact blobs are (0-1)
};

/**
 * @brief Configuration for LineData generation
 */
struct LineDataConfig {
    size_t num_frames = 100;
    size_t lines_per_frame_min = 1;
    size_t lines_per_frame_max = 3;
    size_t points_per_line_min = 10;
    size_t points_per_line_max = 100;
    uint32_t image_width = 640;
    uint32_t image_height = 480;
    int time_start = 0;
    int time_step = 1;
    uint32_t random_seed = 42;

    // Line shape parameters
    bool use_smooth_lines = true;// Generate smooth, curve-like lines
    float smoothness = 0.8f;     // How smooth curves are (0-1)
};

/**
 * @brief Configuration for PointData generation
 */
struct PointDataConfig {
    size_t num_frames = 100;
    size_t points_per_frame_min = 1;
    size_t points_per_frame_max = 10;
    uint32_t image_width = 640;
    uint32_t image_height = 480;
    int time_start = 0;
    int time_step = 1;
    uint32_t random_seed = 42;

    // Point distribution parameters
    bool use_clustered = false;  // Cluster points or distribute uniformly
    float cluster_radius = 50.0f;// Cluster radius if using clustering
};

// ============================================================================
// Preset Configurations
// ============================================================================

/**
 * @brief Preset configurations for common benchmark scenarios
 */
namespace Presets {
// Small data for quick iteration
inline MaskDataConfig SmallMaskData() {
    return MaskDataConfig{
            .num_frames = 10,
            .masks_per_frame_min = 1,
            .masks_per_frame_max = 2,
            .mask_size_min = 50,
            .mask_size_max = 200};
}

// Medium data for realistic testing
inline MaskDataConfig MediumMaskData() {
    return MaskDataConfig{
            .num_frames = 100,
            .masks_per_frame_min = 2,
            .masks_per_frame_max = 5,
            .mask_size_min = 100,
            .mask_size_max = 1000};
}

// Large data for stress testing
inline MaskDataConfig LargeMaskData() {
    return MaskDataConfig{
            .num_frames = 1000,
            .masks_per_frame_min = 5,
            .masks_per_frame_max = 10,
            .mask_size_min = 500,
            .mask_size_max = 5000};
}

// Sparse data (few masks, large gaps)
inline MaskDataConfig SparseMaskData() {
    return MaskDataConfig{
            .num_frames = 1000,
            .masks_per_frame_min = 1,
            .masks_per_frame_max = 1,
            .mask_size_min = 100,
            .mask_size_max = 500,
            .time_step = 10// Masks only every 10 frames
    };
}

// Dense data (many small masks)
inline MaskDataConfig DenseMaskData() {
    return MaskDataConfig{
            .num_frames = 100,
            .masks_per_frame_min = 10,
            .masks_per_frame_max = 20,
            .mask_size_min = 10,
            .mask_size_max = 100};
}
}// namespace Presets

// ============================================================================
// Fixture Classes
// ============================================================================

/**
 * @brief Fixture for generating MaskData
 * 
 * Usage:
 * ```cpp
 * auto fixture = MaskDataFixture(Presets::MediumMaskData());
 * auto mask_data = fixture.generate();
 * ```
 */
class MaskDataFixture {
public:
    explicit MaskDataFixture(MaskDataConfig config = {})
        : config_(config),
          rng_(config.random_seed) {}

    /**
     * @brief Generate a MaskData object according to the configuration
     */
    std::shared_ptr<MaskData> generate() {
        auto mask_data = std::make_shared<MaskData>();

        // Create time frame
        std::vector<int> times;
        times.reserve(config_.num_frames);
        for (size_t i = 0; i < config_.num_frames; ++i) {
            times.push_back(config_.time_start + static_cast<int>(i * config_.time_step));
        }
        auto time_frame = std::make_shared<TimeFrame>(times);
        mask_data->setTimeFrame(time_frame);

        // Generate masks for each frame
        std::uniform_int_distribution<size_t> num_masks_dist(
                config_.masks_per_frame_min, config_.masks_per_frame_max);
        std::uniform_int_distribution<size_t> mask_size_dist(
                config_.mask_size_min, config_.mask_size_max);

        for (size_t frame = 0; frame < config_.num_frames; ++frame) {
            size_t num_masks = num_masks_dist(rng_);
            TimeFrameIndex time_idx(static_cast<int>(frame));

            for (size_t m = 0; m < num_masks; ++m) {
                size_t mask_size = mask_size_dist(rng_);
                Mask2D mask = generateMask(mask_size);
                mask_data->addAtTime(time_idx, mask, NotifyObservers::No);
            }
        }

        return mask_data;
    }

    /**
     * @brief Get the configuration
     */
    MaskDataConfig const & getConfig() const { return config_; }

private:
    /**
     * @brief Generate a single mask with the specified number of pixels
     */
    Mask2D generateMask(size_t num_pixels) {
        std::vector<Point2D<uint32_t>> pixels;
        pixels.reserve(num_pixels);

        if (config_.use_blob_masks) {
            // Generate blob-like mask (clustered pixels)
            pixels = generateBlobMask(num_pixels);
        } else {
            // Generate random scattered pixels
            pixels = generateRandomMask(num_pixels);
        }

        return Mask2D(pixels);
    }

    /**
     * @brief Generate blob-like mask (clustered pixels)
     */
    std::vector<Point2D<uint32_t>> generateBlobMask(size_t num_pixels) {
        std::vector<Point2D<uint32_t>> pixels;

        // Start with a random center point
        std::uniform_int_distribution<uint32_t> x_dist(0, config_.image_width - 1);
        std::uniform_int_distribution<uint32_t> y_dist(0, config_.image_height - 1);

        uint32_t center_x = x_dist(rng_);
        uint32_t center_y = y_dist(rng_);

        // Use a set to avoid duplicate pixels
        std::set<std::pair<uint32_t, uint32_t>> unique_pixels;
        unique_pixels.insert({center_x, center_y});

        // Grow the blob using a random walk with bias toward center
        std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159f);
        std::normal_distribution<float> radius_dist(
                0.0f,
                std::sqrt(static_cast<float>(num_pixels)) * (1.0f - config_.blob_compactness));

        while (unique_pixels.size() < num_pixels) {
            float angle = angle_dist(rng_);
            float radius = std::abs(radius_dist(rng_));

            int32_t dx = static_cast<int32_t>(radius * std::cos(angle));
            int32_t dy = static_cast<int32_t>(radius * std::sin(angle));

            int32_t x = static_cast<int32_t>(center_x) + dx;
            int32_t y = static_cast<int32_t>(center_y) + dy;

            // Clamp to image bounds
            if (x >= 0 && x < static_cast<int32_t>(config_.image_width) &&
                y >= 0 && y < static_cast<int32_t>(config_.image_height)) {
                unique_pixels.insert({static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
            }
        }

        // Convert to vector
        pixels.reserve(unique_pixels.size());
        for (auto const & [x, y]: unique_pixels) {
            pixels.emplace_back(x, y);
        }

        return pixels;
    }

    /**
     * @brief Generate random scattered pixels
     */
    std::vector<Point2D<uint32_t>> generateRandomMask(size_t num_pixels) {
        std::vector<Point2D<uint32_t>> pixels;
        pixels.reserve(num_pixels);

        std::uniform_int_distribution<uint32_t> x_dist(0, config_.image_width - 1);
        std::uniform_int_distribution<uint32_t> y_dist(0, config_.image_height - 1);

        // Use a set to avoid duplicates
        std::set<std::pair<uint32_t, uint32_t>> unique_pixels;

        while (unique_pixels.size() < num_pixels) {
            uint32_t x = x_dist(rng_);
            uint32_t y = y_dist(rng_);
            unique_pixels.insert({x, y});
        }

        // Convert to vector
        for (auto const & [x, y]: unique_pixels) {
            pixels.emplace_back(x, y);
        }

        return pixels;
    }

    MaskDataConfig config_;
    std::mt19937 rng_;
};

/**
 * @brief Fixture for generating LineData
 */
class LineDataFixture {
public:
    explicit LineDataFixture(LineDataConfig config = {})
        : config_(config),
          rng_(config.random_seed) {}

    std::shared_ptr<LineData> generate() {
        auto line_data = std::make_shared<LineData>();

        // Create time frame
        std::vector<int> times;
        times.reserve(config_.num_frames);
        for (size_t i = 0; i < config_.num_frames; ++i) {
            times.push_back(config_.time_start + static_cast<int>(i * config_.time_step));
        }
        auto time_frame = std::make_shared<TimeFrame>(times);
        line_data->setTimeFrame(time_frame);

        // Generate lines for each frame
        std::uniform_int_distribution<size_t> num_lines_dist(
                config_.lines_per_frame_min, config_.lines_per_frame_max);
        std::uniform_int_distribution<size_t> points_per_line_dist(
                config_.points_per_line_min, config_.points_per_line_max);

        for (size_t frame = 0; frame < config_.num_frames; ++frame) {
            size_t num_lines = num_lines_dist(rng_);
            TimeFrameIndex time_idx(static_cast<int>(frame));

            for (size_t l = 0; l < num_lines; ++l) {
                size_t num_points = points_per_line_dist(rng_);
                Line2D line = generateLine(num_points);
                line_data->addAtTime(time_idx, line, NotifyObservers::No);
            }
        }

        return line_data;
    }

    LineDataConfig const & getConfig() const { return config_; }

private:
    Line2D generateLine(size_t num_points) {
        std::vector<Point2D<float>> points;
        points.reserve(num_points);

        if (config_.use_smooth_lines) {
            points = generateSmoothLine(num_points);
        } else {
            points = generateRandomLine(num_points);
        }

        return Line2D(points);
    }

    std::vector<Point2D<float>> generateSmoothLine(size_t num_points) {
        std::vector<Point2D<float>> points;

        // Generate a smooth curve using Bezier-like interpolation
        std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(config_.image_width));
        std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(config_.image_height));

        // Control points
        Point2D<float> start(x_dist(rng_), y_dist(rng_));
        Point2D<float> control1(x_dist(rng_), y_dist(rng_));
        Point2D<float> control2(x_dist(rng_), y_dist(rng_));
        Point2D<float> end(x_dist(rng_), y_dist(rng_));

        // Interpolate
        for (size_t i = 0; i < num_points; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(num_points - 1);

            // Cubic Bezier interpolation
            float x = std::pow(1 - t, 3) * start.x +
                      3 * std::pow(1 - t, 2) * t * control1.x +
                      3 * (1 - t) * std::pow(t, 2) * control2.x +
                      std::pow(t, 3) * end.x;

            float y = std::pow(1 - t, 3) * start.y +
                      3 * std::pow(1 - t, 2) * t * control1.y +
                      3 * (1 - t) * std::pow(t, 2) * control2.y +
                      std::pow(t, 3) * end.y;

            points.emplace_back(x, y);
        }

        return points;
    }

    std::vector<Point2D<float>> generateRandomLine(size_t num_points) {
        std::vector<Point2D<float>> points;

        std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(config_.image_width));
        std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(config_.image_height));

        for (size_t i = 0; i < num_points; ++i) {
            points.emplace_back(x_dist(rng_), y_dist(rng_));
        }

        return points;
    }

    LineDataConfig config_;
    std::mt19937 rng_;
};

/**
 * @brief Fixture for generating PointData
 */
class PointDataFixture {
public:
    explicit PointDataFixture(PointDataConfig config = {})
        : config_(config),
          rng_(config.random_seed) {}

    std::shared_ptr<PointData> generate() {
        auto point_data = std::make_shared<PointData>();

        // Create time frame
        std::vector<int> times;
        times.reserve(config_.num_frames);
        for (size_t i = 0; i < config_.num_frames; ++i) {
            times.push_back(config_.time_start + static_cast<int>(i * config_.time_step));
        }
        auto time_frame = std::make_shared<TimeFrame>(times);
        point_data->setTimeFrame(time_frame);

        // Generate points for each frame
        std::uniform_int_distribution<size_t> num_points_dist(
                config_.points_per_frame_min, config_.points_per_frame_max);

        for (size_t frame = 0; frame < config_.num_frames; ++frame) {
            size_t num_points = num_points_dist(rng_);
            TimeFrameIndex time_idx(static_cast<int>(frame));

            auto points = generatePoints(num_points);
            for (auto const & pt: points) {
                point_data->addAtTime(time_idx, pt, NotifyObservers::No);
            }
        }

        return point_data;
    }

    PointDataConfig const & getConfig() const { return config_; }

private:
    std::vector<Point2D<float>> generatePoints(size_t num_points) {
        std::vector<Point2D<float>> points;
        points.reserve(num_points);

        std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(config_.image_width));
        std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(config_.image_height));

        if (config_.use_clustered) {
            // Generate cluster center
            float center_x = x_dist(rng_);
            float center_y = y_dist(rng_);

            std::normal_distribution<float> offset_dist(0.0f, config_.cluster_radius);

            for (size_t i = 0; i < num_points; ++i) {
                float x = center_x + offset_dist(rng_);
                float y = center_y + offset_dist(rng_);

                // Clamp to bounds
                x = std::max(0.0f, std::min(x, static_cast<float>(config_.image_width - 1)));
                y = std::max(0.0f, std::min(y, static_cast<float>(config_.image_height - 1)));

                points.emplace_back(x, y);
            }
        } else {
            for (size_t i = 0; i < num_points; ++i) {
                points.emplace_back(x_dist(rng_), y_dist(rng_));
            }
        }

        return points;
    }

    PointDataConfig config_;
    std::mt19937 rng_;
};

}// namespace WhiskerToolbox::Benchmark

#endif// WHISKERTOOLBOX_BENCHMARK_FIXTURES_HPP
