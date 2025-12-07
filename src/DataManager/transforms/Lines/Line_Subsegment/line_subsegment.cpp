#include "line_subsegment.hpp"

#include "Lines/Line_Data.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "transforms/utils/variant_type_check.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>


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
        auto const & lines_at_time = line_data->getAtTime(time);
        
        for (auto const & line : lines_at_time) {
            if (line.empty()) {
                continue;
            }
            
            std::vector<Point2D<float>> subsegment;
            
            if (params.method == SubsegmentExtractionMethod::Direct) {
                subsegment = extract_line_subsegment_by_distance(
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
                result_line_data->addAtTime(time, subsegment, NotifyObservers::No);
            }
        }
        
        processed_times++;
        int progress = static_cast<int>(
            std::round(static_cast<double>(processed_times) / static_cast<double>(times_with_data.size()) * 100.0)
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
    return canApplyToType<LineData>(dataVariant);
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
