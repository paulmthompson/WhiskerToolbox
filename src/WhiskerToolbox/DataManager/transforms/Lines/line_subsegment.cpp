#include "line_subsegment.hpp"

#include "Lines/Line_Data.hpp"
#include "Lines/utils/line_geometry.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

/**
 * @brief Extract subsegment using direct point selection based on distance
 */
std::vector<Point2D<float>> extract_direct_subsegment(
        Line2D const & line,
        float start_pos,
        float end_pos,
        bool preserve_spacing) {
    
    // Use the new distance-based utility function
    return extract_line_subsegment_by_distance(line, start_pos, end_pos, preserve_spacing);
}

/**
 * @brief Extract subsegment using parametric polynomial interpolation with distance-based positioning
 */
std::vector<Point2D<float>> extract_parametric_subsegment(
        Line2D const & line,
        float start_pos,
        float end_pos,
        int polynomial_order,
        int output_points) {
    
    if (line.empty()) {
        return {};
    }
    
    if (line.size() == 1) {
        return {line[0]};
    }
    
    // Ensure valid range
    start_pos = std::max(0.0f, std::min(1.0f, start_pos));
    end_pos = std::max(0.0f, std::min(1.0f, end_pos));
    
    if (start_pos >= end_pos) {
        return {};
    }
    
    // Need enough points for polynomial fitting
    if (line.size() < static_cast<size_t>(polynomial_order + 1)) {
        // Fall back to direct method
        return extract_direct_subsegment(line, start_pos, end_pos, false);
    }
    
    // Calculate cumulative distances for distance-based parameterization
    std::vector<float> distances = calc_cumulative_length_vector(line);
    float total_length = distances.back();
    
    if (total_length < 1e-6f) {
        // Line has no length, return first point
        return {line[0]};
    }
    
    // Convert fractional positions to actual distances
    float start_distance = start_pos * total_length;
    float end_distance = end_pos * total_length;
    
    // Create normalized t-values based on cumulative distance
    std::vector<double> t_values;
    t_values.reserve(line.size());
    for (float distance : distances) {
        t_values.push_back(static_cast<double>(distance / total_length));
    }
    
    // Extract coordinates
    std::vector<double> x_coords, y_coords;
    x_coords.reserve(line.size());
    y_coords.reserve(line.size());
    
    for (auto const & point : line) {
        x_coords.push_back(static_cast<double>(point.x));
        y_coords.push_back(static_cast<double>(point.y));
    }
    
    // Fit parametric polynomials
    std::vector<double> x_coeffs = fit_single_dimension_polynomial_internal(x_coords, t_values, polynomial_order);
    std::vector<double> y_coeffs = fit_single_dimension_polynomial_internal(y_coords, t_values, polynomial_order);
    
    if (x_coeffs.empty() || y_coeffs.empty()) {
        // Fall back to direct method if fitting failed
        return extract_direct_subsegment(line, start_pos, end_pos, false);
    }
    
    // Generate subsegment points using distance-based parameterization
    std::vector<Point2D<float>> subsegment;
    subsegment.reserve(output_points);
    
    for (int i = 0; i < output_points; ++i) {
        // Map from output point index to t-parameter in the subsegment range
        double t_local = static_cast<double>(i) / static_cast<double>(output_points - 1);
        double t_global = start_pos + t_local * (end_pos - start_pos);
        
        // Evaluate polynomials at this t-value
        double x = evaluate_polynomial(x_coeffs, t_global);
        double y = evaluate_polynomial(y_coeffs, t_global);
        
        subsegment.push_back({static_cast<float>(x), static_cast<float>(y)});
    }
    
    return subsegment;
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LineData> extract_line_subsegment(
        LineData const * line_data,
        LineSubsegmentParameters const & params) {
    return extract_line_subsegment(line_data, params, [](int){});
}

std::shared_ptr<LineData> extract_line_subsegment(
        LineData const * line_data,
        LineSubsegmentParameters const & params,
        ProgressCallback progressCallback) {
    
    auto result_line_data = std::make_shared<LineData>();
    
    if (!line_data) {
        progressCallback(100);
        return result_line_data;
    }
    
    // Copy image size
    result_line_data->setImageSize(line_data->getImageSize());
    
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
            if (line.empty()) {
                continue;
            }
            
            std::vector<Point2D<float>> subsegment;
            
            if (params.method == SubsegmentExtractionMethod::Direct) {
                subsegment = extract_direct_subsegment(
                    line,
                    params.start_position,
                    params.end_position,
                    params.preserve_original_spacing
                );
            } else { // Parametric
                subsegment = extract_parametric_subsegment(
                    line,
                    params.start_position,
                    params.end_position,
                    params.polynomial_order,
                    params.output_points
                );
            }
            
            if (!subsegment.empty()) {
                result_line_data->addLineAtTime(time, subsegment, false);
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

std::string LineSubsegmentOperation::getName() const {
    return "Extract Line Subsegment";
}

std::type_index LineSubsegmentOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineSubsegmentOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> LineSubsegmentOperation::getDefaultParameters() const {
    return std::make_unique<LineSubsegmentParameters>();
}

DataTypeVariant LineSubsegmentOperation::execute(DataTypeVariant const & dataVariant,
                                                TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant LineSubsegmentOperation::execute(DataTypeVariant const & dataVariant,
                                                TransformParametersBase const * transformParameters,
                                                ProgressCallback progressCallback) {
    
    auto const * line_data_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    
    if (!line_data_ptr || !(*line_data_ptr)) {
        std::cerr << "LineSubsegmentOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }
    
    LineData const * input_line_data = (*line_data_ptr).get();
    
    LineSubsegmentParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;
    
    if (transformParameters) {
        params = dynamic_cast<LineSubsegmentParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "LineSubsegmentOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<LineSubsegmentParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = static_cast<LineSubsegmentParameters const *>(default_params_owner.get());
    }
    
    if (!params) {
        std::cerr << "LineSubsegmentOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }
    
    std::shared_ptr<LineData> result = extract_line_subsegment(input_line_data, *params, progressCallback);
    
    if (!result) {
        std::cerr << "LineSubsegmentOperation::execute: 'extract_line_subsegment' failed to produce a result." << std::endl;
        return {};
    }
    
    std::cout << "LineSubsegmentOperation executed successfully." << std::endl;
    return result;
} 
