#include "line_point_extraction.hpp"

#include "Lines/Line_Data.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "Points/Point_Data.hpp"
#include "transforms/utils/variant_type_check.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"
#include "utils/polynomial/polynomial_fit.hpp"


#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>



///////////////////////////////////////////////////////////////////////////////

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
    for (auto time : times_with_data) {
        auto const & lines_at_time = line_data->getAtTime(time);
        
        // Process only the first line at each time point (similar to other line operations)
        if (!lines_at_time.empty()) {
            auto const & line = lines_at_time[0];
            
            if (!line.empty()) {
                std::optional<Point2D<float>> extracted_point;
                
                if (params.method == PointExtractionMethod::Direct) {
                    extracted_point = point_at_fractional_position(
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
                    result_point_data->addAtTime(time, extracted_point.value(), NotifyObservers::No);
                }
            }
        }
        
        processed_times++;
        int progress = static_cast<int>(
            std::round(static_cast<double>(processed_times) / static_cast<double>(times_with_data.size()) * 100.0)
        );
        progressCallback(progress);
    }
    
    progressCallback(100);
    return result_point_data;
}

///////////////////////////////////////////////////////////////////////////////

std::string LinePointExtractionOperation::getName() const {
    return "Extract Point from Line";
}

std::type_index LinePointExtractionOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LinePointExtractionOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
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
