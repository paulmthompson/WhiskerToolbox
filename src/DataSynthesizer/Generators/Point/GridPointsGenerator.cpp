/**
 * @file GridPointsGenerator.cpp
 * @brief PointData generator that produces a regular grid of points repeated for each frame.
 *
 * Registers a "GridPoints" generator in the DataSynthesizer registry.
 * Produces a PointData where each frame contains a grid of points defined by
 * rows, columns, spacing, and origin.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct GridPointsParams {
    int rows = 5;
    int cols = 5;
    float spacing = 20.0f;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    int num_frames = 100;
};

DataTypeVariant generateGridPoints(GridPointsParams const & params) {
    if (params.rows <= 0) {
        throw std::invalid_argument("GridPoints: rows must be > 0");
    }
    if (params.cols <= 0) {
        throw std::invalid_argument("GridPoints: cols must be > 0");
    }
    if (params.spacing <= 0.0f) {
        throw std::invalid_argument("GridPoints: spacing must be > 0");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("GridPoints: num_frames must be > 0");
    }

    std::vector<Point2D<float>> grid;
    grid.reserve(static_cast<size_t>(params.rows) * static_cast<size_t>(params.cols));

    for (int r = 0; r < params.rows; ++r) {
        for (int c = 0; c < params.cols; ++c) {
            float const x = params.origin_x + static_cast<float>(c) * params.spacing;
            float const y = params.origin_y + static_cast<float>(r) * params.spacing;
            grid.emplace_back(x, y);
        }
    }

    auto point_data = std::make_shared<PointData>();

    for (int i = 0; i < params.num_frames; ++i) {
        point_data->addAtTime(TimeFrameIndex(i), grid, NotifyObservers::No);
    }

    return point_data;
}

auto const grid_points_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<GridPointsParams>(
                "GridPoints",
                generateGridPoints,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a regular grid of points repeated at every frame. "
                                       "The grid is defined by rows, columns, spacing between points, "
                                       "and an origin position.",
                        .category = "Spatial",
                        .output_type = "PointData"});

}// namespace
