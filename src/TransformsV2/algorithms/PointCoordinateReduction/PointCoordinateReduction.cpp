/**
 * @file PointCoordinateReduction.cpp
 * @brief Implementation of PointData coordinate projection with ragged dimension reduction.
 */

#include "PointCoordinateReduction.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Points/Point_Data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <cmath>
#include <limits>
#include <vector>

namespace Neuralyzer::Transforms::V2::Examples {

namespace {

[[nodiscard]] float coordinateValue(
        Point2D<float> const & point,
        PointCoordinateComponent coordinate) {
    switch (coordinate) {
        case PointCoordinateComponent::X:
            return point.x;
        case PointCoordinateComponent::Y:
            return point.y;
    }
    return point.x;
}

}// namespace

std::shared_ptr<AnalogTimeSeries> pointCoordinateReduction(
        PointData const & input,
        PointCoordinateReductionParams const & params,
        ComputeContext const & ctx) {

    // First pass to determine the exact number of unique timeframes
    // so we can reserve our vector exactly
    auto times_with_data = input.getTimesWithData();
    std::size_t num_frames = times_with_data.size();
    
    std::vector<float> values;
    std::vector<TimeFrameIndex> times;
    values.reserve(num_frames);
    times.reserve(num_frames);

    std::size_t processed = 0;

    TimeFrameIndex current_time(-1);
    bool has_current_time = false;
    float current_sum = 0.0f;
    float current_max = -std::numeric_limits<float>::infinity();
    float current_min = std::numeric_limits<float>::infinity();
    float current_first = 0.0f;
    int current_count = 0;

    auto push_current_time = [&]() {
        if (!has_current_time || current_count == 0) return;
        
        times.push_back(current_time);
        
        switch (params.reduction) {
            case PointReductionMethod::First:
                values.push_back(current_first);
                break;
            case PointReductionMethod::Mean:
                values.push_back(current_sum / current_count);
                break;
            case PointReductionMethod::Sum:
                values.push_back(current_sum);
                break;
            case PointReductionMethod::Max:
                values.push_back(current_max);
                break;
            case PointReductionMethod::Min:
                values.push_back(current_min);
                break;
            default:
                values.push_back(current_first);
                break;
        }
        
        current_sum = 0.0f;
        current_max = -std::numeric_limits<float>::infinity();
        current_min = std::numeric_limits<float>::infinity();
        current_count = 0;
    };

    for (auto const & [time, entry]: input.elements()) {
        if (ctx.shouldCancel()) {
            return nullptr;
        }

        if (has_current_time && time != current_time) {
            push_current_time();
        }
        
        current_time = time;
        has_current_time = true;
        
        // Notice `entry.data` here based on how PointCoordinate works
        float val = coordinateValue(entry.data, params.coordinate);
        
        if (current_count == 0) {
            current_first = val;
        }
        current_sum += val;
        if (val > current_max) current_max = val;
        if (val < current_min) current_min = val;
        
        ++current_count;
        ++processed;
    }
    
    push_current_time();

    if (processed > 0) {
        ctx.reportProgress(100);
    }
    
    auto output = std::make_shared<AnalogTimeSeries>(std::move(values), std::move(times));
    output->setTimeFrame(input.getTimeFrame());
    return output;
}

}// namespace Neuralyzer::Transforms::V2::Examples
