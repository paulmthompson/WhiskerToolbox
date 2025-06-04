#include "line_point_extraction.hpp"

#include "Lines/Line_Data.hpp"
#include "Lines/utils/line_geometry.hpp"
#include "Points/Point_Data.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"
#include "utils/polynomial/polynomial_fit.hpp"


#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

/**
 * @brief Extract point using direct point selection based on distance
 */
std::optional<Point2D<float>> extract_direct_point(
        Line2D const & line,
        float position,
        bool use_interpolation) {
    
    // Use the new distance-based utility function
    return point_at_fractional_position(line, position, use_interpolation);
}

/**
 * @brief Extract point using parametric polynomial interpolation
 */
std::optional<Point2D<float>> extract_parametric_point(
        Line2D const & line,
        float position,
        int polynomial_order) {
    
    if (line.empty()) {
        return std::nullopt;
    }
    
    if (line.size() == 1) {
        return line[0];
    }
    
    // Ensure valid range
    position = std::max(0.0f, std::min(1.0f, position));
    
    // Need enough points for polynomial fitting
    if (line.size() < static_cast<size_t>(polynomial_order + 1)) {
        // Fall back to direct method
        return extract_direct_point(line, position, true);
    }
    
    // Compute t-values for the entire line
    std::vector<double> t_values = compute_t_values(line);
    if (t_values.empty()) {
        return extract_direct_point(line, position, true);
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
        return extract_direct_point(line, position, true);
    }
    
    // Evaluate polynomials at the specified position
    double t_eval = static_cast<double>(position);
    double x = evaluate_polynomial(x_coeffs, t_eval);
    double y = evaluate_polynomial(y_coeffs, t_eval);
    
    return Point2D<float>{static_cast<float>(x), static_cast<float>(y)};
}

std::shared_ptr<PointData> extract_line_point(
        LineData const * line_data,
        LinePointExtractionParameters const & params) {
    return extract_line_point(line_data, params, [](int){});
}

std::shared_ptr<PointData> extract_line_point(
        LineData const * line_data,
        LinePointExtractionParameters const & params,
        ProgressCallback progressCallback) {
    
    auto result_point_data = std::make_shared<PointData>();
    
    if (!line_data) {
        progressCallback(100);
        return result_point_data;
    }
    
    // Copy image size
    result_point_data->setImageSize(line_data->getImageSize());
    
    // Get all times with data for progress calculation
    auto times_with_data = line_data->getTimesWithData();
    if (times_with_data.empty()) {
        progressCallback(100);
        return result_point_data;
    }
    
    progressCallback(0);
    
    size_t processed_times = 0;
    for (int time : times_with_data) {
        auto const & lines_at_time = line_data->getLinesAtTime(time);
        
        // Process only the first line at each time point (similar to other line operations)
        if (!lines_at_time.empty()) {
            auto const & line = lines_at_time[0];
            
            if (!line.empty()) {
                std::optional<Point2D<float>> extracted_point;
                
                if (params.method == PointExtractionMethod::Direct) {
                    extracted_point = extract_direct_point(
                        line,
                        params.position,
                        params.use_interpolation
                    );
                } else { // Parametric
                    extracted_point = extract_parametric_point(
                        line,
                        params.position,
                        params.polynomial_order
                    );
                }
                
                if (extracted_point.has_value()) {
                    result_point_data->addPointAtTime(time, extracted_point.value(), false);
                }
            }
        }
        
        processed_times++;
        int progress = static_cast<int>(
            std::round(static_cast<double>(processed_times) / times_with_data.size() * 100.0)
        );
        progressCallback(progress);
    }
    
    progressCallback(100);
    return result_point_data;
}

// LinePointExtractionOperation implementation

std::string LinePointExtractionOperation::getName() const {
    return "Extract Point from Line";
}

std::type_index LinePointExtractionOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LinePointExtractionOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> LinePointExtractionOperation::getDefaultParameters() const {
    return std::make_unique<LinePointExtractionParameters>();
}

DataTypeVariant LinePointExtractionOperation::execute(DataTypeVariant const & dataVariant,
                                                     TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant LinePointExtractionOperation::execute(DataTypeVariant const & dataVariant,
                                                     TransformParametersBase const * transformParameters,
                                                     ProgressCallback progressCallback) {
    
    auto const * line_data_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    
    if (!line_data_ptr || !(*line_data_ptr)) {
        std::cerr << "LinePointExtractionOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }
    
    LineData const * input_line_data = (*line_data_ptr).get();
    
    LinePointExtractionParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;
    
    if (transformParameters) {
        params = dynamic_cast<LinePointExtractionParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "LinePointExtractionOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<LinePointExtractionParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = static_cast<LinePointExtractionParameters const *>(default_params_owner.get());
    }
    
    if (!params) {
        std::cerr << "LinePointExtractionOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }
    
    std::shared_ptr<PointData> result = extract_line_point(input_line_data, *params, progressCallback);
    
    if (!result) {
        std::cerr << "LinePointExtractionOperation::execute: 'extract_line_point' failed to produce a result." << std::endl;
        return {};
    }
    
    std::cout << "LinePointExtractionOperation executed successfully." << std::endl;
    return result;
} 
