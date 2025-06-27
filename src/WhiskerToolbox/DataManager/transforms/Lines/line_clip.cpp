#include "line_clip.hpp"

#include "Lines/Line_Data.hpp"

#include <cmath>
#include <iostream>
#include <vector>

std::optional<Point2D<float>> line_segment_intersection(
    Point2D<float> const & p1, Point2D<float> const & p2,
    Point2D<float> const & p3, Point2D<float> const & p4) {
    
    // Calculate direction vectors
    float d1x = p2.x - p1.x;
    float d1y = p2.y - p1.y;
    float d2x = p4.x - p3.x;
    float d2y = p4.y - p3.y;
    
    // Calculate the denominator for the intersection calculation
    float denominator = d1x * d2y - d1y * d2x;
    
    // Check if lines are parallel (denominator is zero)
    if (std::abs(denominator) < 1e-10f) {
        return std::nullopt;
    }
    
    // Calculate parameters for intersection point
    float dx = p1.x - p3.x;
    float dy = p1.y - p3.y;
    
    float t1 = (dx * d2y - dy * d2x) / denominator;
    float t2 = (dx * d1y - dy * d1x) / denominator;
    
    // Check if intersection point lies within both line segments
    if (t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f) {
        // Calculate intersection point
        Point2D<float> intersection;
        intersection.x = p1.x + t1 * d1x;
        intersection.y = p1.y + t1 * d1y;
        return intersection;
    }
    
    return std::nullopt;
}

std::optional<std::pair<Point2D<float>, size_t>> find_line_intersection(
    Line2D const & line, Line2D const & reference_line) {
    
    if (line.size() < 2 || reference_line.size() < 2) {
        return std::nullopt;
    }
    
    // Check each segment of the main line against each segment of the reference line
    for (size_t i = 0; i < line.size() - 1; ++i) {
        for (size_t j = 0; j < reference_line.size() - 1; ++j) {
            auto intersection = line_segment_intersection(
                line[i], line[i + 1],
                reference_line[j], reference_line[j + 1]
            );
            
            if (intersection.has_value()) {
                return std::make_pair(intersection.value(), i);
            }
        }
    }
    
    return std::nullopt;
}

Line2D clip_line_at_intersection(
    Line2D const & line, 
    Line2D const & reference_line, 
    ClipSide clip_side) {
    
    if (line.size() < 2) {
        return line; // Return original line if too short
    }
    
    auto intersection_result = find_line_intersection(line, reference_line);
    
    if (!intersection_result.has_value()) {
        // No intersection found, return original line
        return line;
    }
    
    Point2D<float> intersection_point = intersection_result->first;
    size_t segment_index = intersection_result->second;
    
    Line2D clipped_line;
    
    if (clip_side == ClipSide::KeepBase) {
        // Keep from start to intersection
        for (size_t i = 0; i <= segment_index; ++i) {
            clipped_line.push_back(line[i]);
        }
        // Add intersection point if it's not the same as the last point
        if (clipped_line.empty() || 
            (std::abs(clipped_line.back().x - intersection_point.x) > 1e-6f ||
             std::abs(clipped_line.back().y - intersection_point.y) > 1e-6f)) {
            clipped_line.push_back(intersection_point);
        }
    } else { // ClipSide::KeepDistal
        // Keep from intersection to end
        clipped_line.push_back(intersection_point);
        for (size_t i = segment_index + 1; i < line.size(); ++i) {
            clipped_line.push_back(line[i]);
        }
    }
    
    return clipped_line;
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LineData> clip_lines(
    LineData const * line_data,
    LineClipParameters const * params) {
    return clip_lines(line_data, params, [](int){});
}

std::shared_ptr<LineData> clip_lines(
    LineData const * line_data,
    LineClipParameters const * params,
    ProgressCallback progressCallback) {
    
    auto result_line_data = std::make_shared<LineData>();
    
    if (!line_data || !params || !params->reference_line_data) {
        std::cerr << "LineClip: Invalid input parameters." << std::endl;
        progressCallback(100);
        return result_line_data;
    }
    
    // Copy image size from input
    result_line_data->setImageSize(line_data->getImageSize());
    
    // Get reference line from the specified frame
    auto const & reference_lines = params->reference_line_data->getLinesAtTime(TimeFrameIndex(params->reference_frame));
    if (reference_lines.empty()) {
        std::cerr << "LineClip: No reference line found at frame " << params->reference_frame << std::endl;
        progressCallback(100);
        return result_line_data;
    }
    
    // Use the first line from the reference frame
    Line2D const & reference_line = reference_lines[0];
    
    // Get all times with data for progress calculation
    auto times_with_data = line_data->getTimesWithData();
    if (times_with_data.empty()) {
        progressCallback(100);
        return result_line_data;
    }
    
    progressCallback(0);
    
    size_t processed_times = 0;
    
    for (auto time : times_with_data) {
        auto const & lines_at_time = line_data->getLinesAtTime(time);
        
        for (auto const & line : lines_at_time) {
            if (line.size() < 2) {
                continue; // Skip lines that are too short
            }
            
            // Clip the line using the reference line
            Line2D clipped_line = clip_line_at_intersection(line, reference_line, params->clip_side);
            
            // Add clipped line to result (only if it has at least 2 points)
            if (clipped_line.size() >= 2) {
                result_line_data->addLineAtTime(time, clipped_line, false);
            }
        }
        
        processed_times++;
        int progress = static_cast<int>(
            std::round(static_cast<double>(processed_times) / times_with_data.size() * 100.0)
        );
        progressCallback(progress);
    }
    
    progressCallback(100);
    return result_line_data;
}

///////////////////////////////////////////////////////////////////////////////

std::string LineClipOperation::getName() const {
    return "Clip Line by Reference Line";
}

std::type_index LineClipOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineClipOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> LineClipOperation::getDefaultParameters() const {
    return std::make_unique<LineClipParameters>();
}

DataTypeVariant LineClipOperation::execute(DataTypeVariant const & dataVariant,
                                          TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant LineClipOperation::execute(DataTypeVariant const & dataVariant,
                                          TransformParametersBase const * transformParameters,
                                          ProgressCallback progressCallback) {
    
    auto const * line_data_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    
    if (!line_data_ptr || !(*line_data_ptr)) {
        std::cerr << "LineClipOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }
    
    LineData const * input_line_data = (*line_data_ptr).get();
    
    LineClipParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;
    
    if (transformParameters) {
        params = dynamic_cast<LineClipParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "LineClipOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<LineClipParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = static_cast<LineClipParameters const *>(default_params_owner.get());
    }
    
    if (!params) {
        std::cerr << "LineClipOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }
    
    std::shared_ptr<LineData> result = clip_lines(input_line_data, params, progressCallback);
    
    if (!result) {
        std::cerr << "LineClipOperation::execute: 'clip_lines' failed to produce a result." << std::endl;
        return {};
    }
    
    std::cout << "LineClipOperation executed successfully." << std::endl;
    return result;
} 
