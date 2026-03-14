/**
 * @file StraightLineGenerator.cpp
 * @brief LineData generator that produces a straight line segment repeated for each frame.
 *
 * Registers a "StraightLine" generator in the DataSynthesizer registry.
 * Produces a LineData where each frame contains a single line interpolated
 * between start and end points with a specified number of sample points.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct StraightLineParams {
    float start_x = 0.0f;
    float start_y = 0.0f;
    float end_x = 100.0f;
    float end_y = 100.0f;
    int num_points_per_line = 50;
    int num_frames = 100;
};

DataTypeVariant generateStraightLine(StraightLineParams const & params) {
    if (params.num_points_per_line <= 1) {
        throw std::invalid_argument("StraightLine: num_points_per_line must be > 1");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("StraightLine: num_frames must be > 0");
    }

    auto const n = static_cast<size_t>(params.num_points_per_line);
    std::vector<Point2D<float>> points;
    points.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(n - 1);
        float const x = params.start_x + t * (params.end_x - params.start_x);
        float const y = params.start_y + t * (params.end_y - params.start_y);
        points.emplace_back(x, y);
    }
    Line2D const line(std::move(points));

    auto line_data = std::make_shared<LineData>();

    for (int i = 0; i < params.num_frames; ++i) {
        line_data->addAtTime(TimeFrameIndex(i), line, NotifyObservers::No);
    }

    return line_data;
}

auto const straight_line_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<StraightLineParams>(
                "StraightLine",
                generateStraightLine,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a straight line segment repeated at every frame. "
                                       "The line is defined by start and end points, with "
                                       "num_points_per_line samples linearly interpolated between them.",
                        .category = "Spatial",
                        .output_type = "LineData"});

}// namespace
