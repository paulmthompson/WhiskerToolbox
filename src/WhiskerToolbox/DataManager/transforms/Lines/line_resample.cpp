#include "line_resample.hpp"

#include "Lines/Line_Data.hpp"
#include "Lines/utils/line_resampling.hpp"// For the actual resampling

#include <iostream> // For error messages / cout
#include <map> // for std::map

std::string LineResampleOperation::getName() const {
    return "Resample Line";
}

std::type_index LineResampleOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineResampleOperation::canApply(DataTypeVariant const& dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }
    auto const* ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr; // Check if it's a valid, non-null LineData
}

std::unique_ptr<TransformParametersBase> LineResampleOperation::getDefaultParameters() const {
    return std::make_unique<LineResampleParameters>();
}

DataTypeVariant LineResampleOperation::execute(DataTypeVariant const& dataVariant,
                                           TransformParametersBase const* transformParameters) {
    // Call the version with progress reporting but ignore progress here
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant LineResampleOperation::execute(DataTypeVariant const& dataVariant,
                                           TransformParametersBase const* transformParameters,
                                           ProgressCallback progressCallback) {
    auto const* line_data_sptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    if (!line_data_sptr_ptr || !(*line_data_sptr_ptr)) {
        std::cerr << "LineResampleOperation::execute called with incompatible variant type or null data." << std::endl;
        return {}; // Return empty variant
    }
    LineData const* input_line_data = (*line_data_sptr_ptr).get();

    LineResampleParameters const* params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner; // Keep owner alive

    if (transformParameters) {
        params = dynamic_cast<LineResampleParameters const*>(transformParameters);
        if (!params) {
            std::cerr << "LineResampleOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<LineResampleParameters const*>(default_params_owner.get());
        }
    } else {
         default_params_owner = getDefaultParameters();
         params = static_cast<LineResampleParameters const*>(default_params_owner.get());
    }
    
    if (!params) { // Should not happen if getDefaultParameters is correct and dynamic_cast was not null before
        std::cerr << "LineResampleOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }
    
    float target_spacing = params->target_spacing;
    std::cout << "LineResampleOperation: target_spacing = " << target_spacing << std::endl;

    auto resampled_line_map = std::map<TimeFrameIndex, std::vector<Line2D>>();
    int total_lines = 0;
    for (auto const& time_lines_pair : input_line_data->GetAllLinesAsRange()) {
        total_lines += time_lines_pair.lines.size();
    }
    if (total_lines == 0) {
        progressCallback(100);
        auto empty_result = std::make_shared<LineData>();
        empty_result->setImageSize(input_line_data->getImageSize());
        return empty_result;
    }

    int processed_lines = 0;
    progressCallback(0);

    for (auto const& time_lines_pair : input_line_data->GetAllLinesAsRange()) {
        TimeFrameIndex time = time_lines_pair.time;
        std::vector<Line2D> new_lines_at_time;
        new_lines_at_time.reserve(time_lines_pair.lines.size());

        for (const auto& single_line : time_lines_pair.lines) {
            if (single_line.empty()) {
                new_lines_at_time.push_back(Line2D()); // Keep empty lines as empty
            } else {
                new_lines_at_time.push_back(resample_line_points(single_line, target_spacing));
            }
            processed_lines++;
            if (total_lines > 0) {
                 progressCallback(static_cast<int>((static_cast<double>(processed_lines) / total_lines) * 100.0));
            }
        }
        if (!new_lines_at_time.empty() || input_line_data->getAtTime(time).empty()) {
             // Add to map if there are new lines OR if the original time had lines (even if all became empty)
             // This ensures that times that previously had data but now have all empty lines are still represented.
             // However, if a time originally had no lines, we don't add an empty entry for it.
            if (!input_line_data->getAtTime(time).empty() || !new_lines_at_time.empty()) {
                 resampled_line_map[time] = std::move(new_lines_at_time);
            }
        }
    }

    auto result_line_data = std::make_shared<LineData>(resampled_line_map);
    result_line_data->setImageSize(input_line_data->getImageSize()); // Preserve image size

    progressCallback(100);
    std::cout << "LineResampleOperation executed successfully." << std::endl;
    return result_line_data;
} 
