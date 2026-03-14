/**
 * @file RandomPointsGenerator.cpp
 * @brief PointData generator that produces random points repeated for each frame.
 *
 * Registers a "RandomPoints" generator in the DataSynthesizer registry.
 * Produces a PointData where each frame contains the same set of uniformly
 * distributed random points within a bounding box.
 * Deterministic: same seed always produces the same output.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct RandomPointsParams {
    int num_points = 20;
    float min_x = 0.0f;
    float max_x = 640.0f;
    float min_y = 0.0f;
    float max_y = 480.0f;
    int num_frames = 100;
    std::optional<uint64_t> seed;
};

DataTypeVariant generateRandomPoints(RandomPointsParams const & params) {
    if (params.num_points <= 0) {
        throw std::invalid_argument("RandomPoints: num_points must be > 0");
    }
    if (params.max_x <= params.min_x) {
        throw std::invalid_argument("RandomPoints: max_x must be > min_x");
    }
    if (params.max_y <= params.min_y) {
        throw std::invalid_argument("RandomPoints: max_y must be > min_y");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("RandomPoints: num_frames must be > 0");
    }

    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::uniform_real_distribution<float> dist_x(params.min_x, params.max_x);
    std::uniform_real_distribution<float> dist_y(params.min_y, params.max_y);

    std::vector<Point2D<float>> points;
    points.reserve(static_cast<size_t>(params.num_points));

    for (int p = 0; p < params.num_points; ++p) {
        points.emplace_back(dist_x(rng), dist_y(rng));
    }

    auto point_data = std::make_shared<PointData>();

    for (int i = 0; i < params.num_frames; ++i) {
        point_data->addAtTime(TimeFrameIndex(i), points, NotifyObservers::No);
    }

    return point_data;
}

auto const random_points_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<RandomPointsParams>(
                "RandomPoints",
                generateRandomPoints,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates uniformly distributed random points repeated at "
                                       "every frame. Points are sampled within a bounding box "
                                       "[min_x, max_x] × [min_y, max_y]. "
                                       "Deterministic: same seed always produces the same output.",
                        .category = "Spatial",
                        .output_type = "PointData"});

}// namespace
