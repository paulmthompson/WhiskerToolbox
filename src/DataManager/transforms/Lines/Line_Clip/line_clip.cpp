#include "line_clip.hpp"

#include "Lines/Line_Data.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <cmath>
#include <iostream>
#include <vector>


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
    auto const & reference_lines = params->reference_line_data->getAtTime(TimeFrameIndex(params->reference_frame));
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
        auto const & lines_at_time = line_data->getAtTime(time);
        
        for (auto const & line : lines_at_time) {
            if (line.size() < 2) {
                continue; // Skip lines that are too short
            }
            
            // Clip the line using the reference line
            Line2D clipped_line = clip_line_at_intersection(line, reference_line, params->clip_side);
            
            // Add clipped line to result (only if it has at least 2 points)
            if (clipped_line.size() >= 2) {
                result_line_data->addAtTime(time, clipped_line, NotifyObservers::No);
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

std::string LineClipOperation::getName() const {
    return "Clip Line by Reference Line";
}

std::type_index LineClipOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineClipOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
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
    
    return result;
} 
