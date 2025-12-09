#include "line_curvature.hpp"


#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/utils/variant_type_check.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"

#include <cmath>   // std::sqrt, std::pow, std::abs, NAN
#include <iostream>// For std::cerr
#include <map>     // For std::map
#include <vector>  // For std::vector


///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<AnalogTimeSeries> line_curvature(
        LineData const * line_data,
        LineCurvatureParameters const * params) {

    return line_curvature(line_data, params, [](int) { /* Default no-op progress */ });
}

std::shared_ptr<AnalogTimeSeries> line_curvature(
        LineData const * line_data,
        LineCurvatureParameters const * params,
        ProgressCallback progressCallback) {

    std::map<int, float> curvatures_map;

    if (!line_data || !params) {
        std::cerr << "LineCurvature: Null LineData or parameters provided." << std::endl;
        progressCallback(100);                      // Still call progress to complete
        return std::make_shared<AnalogTimeSeries>();// Return empty series
    }

    // Determine total number of time points for progress calculation
    size_t total_time_points = line_data->getTotalEntryCount();
    if (total_time_points == 0) {
        progressCallback(100);
        return std::make_shared<AnalogTimeSeries>();
    }

    size_t processed_time_points = 0;
    progressCallback(0);// Initial progress

    float position = params->position;
    int polynomial_order = params->polynomial_order;
    float fitting_window = params->fitting_window_percentage;// Get fitting_window again

    for (auto const & [time, entity_id, line]: line_data->flattened_data()) {

        if (line.size() < 2) {// Need at least two points to define a direction/curvature
            processed_time_points++;
            int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * 100.0));
            progressCallback(current_progress);
            continue;
        }

        std::optional<float> curvature_val;
        if (params->method == CurvatureCalculationMethod::PolynomialFit) {
            curvature_val = calculate_polynomial_curvature(line, position, polynomial_order, fitting_window);// Pass fitting_window
        }
        // else if (params->method == AnotherMethod) { ... }

        if (curvature_val.has_value()) {
            curvatures_map[static_cast<int>(time.getValue())] = curvature_val.value();
        }
        // If curvature_val is std::nullopt, this time point is simply omitted from the AnalogTimeSeries

        processed_time_points++;
        int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * 100.0));
        progressCallback(current_progress);
    }

    progressCallback(100);// Final progress update
    return std::make_shared<AnalogTimeSeries>(curvatures_map);
}

///////////////////////////////////////////////////////////////////////////////

std::string LineCurvatureOperation::getName() const {
    return "Calculate Line Curvature";
}

std::type_index LineCurvatureOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineCurvatureOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineCurvatureOperation::getDefaultParameters() const {
    return std::make_unique<LineCurvatureParameters>();
}

DataTypeVariant LineCurvatureOperation::execute(DataTypeVariant const & dataVariant,
                                                TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) { /* Default no-op progress */ });
}

DataTypeVariant LineCurvatureOperation::execute(DataTypeVariant const & dataVariant,
                                                TransformParametersBase const * transformParameters,
                                                ProgressCallback progressCallback) {
    auto const * line_data_sptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    if (!line_data_sptr_ptr || !(*line_data_sptr_ptr)) {
        std::cerr << "LineCurvatureOperation::execute: Incompatible variant type or null data." << std::endl;
        return {};
    }
    LineData const * input_line_data = (*line_data_sptr_ptr).get();

    LineCurvatureParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;

    if (transformParameters) {
        params = dynamic_cast<LineCurvatureParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "LineCurvatureOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<LineCurvatureParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = static_cast<LineCurvatureParameters const *>(default_params_owner.get());
    }

    if (!params) {
        std::cerr << "LineCurvatureOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }

    // Simple progress: 0 at start, 50 during, 100 at end, as actual items are lines within times
    // progressCallback(0); // This is now handled inside line_curvature
    std::shared_ptr<AnalogTimeSeries> result_ts = line_curvature(input_line_data, params, progressCallback);// Pass callback
    // progressCallback(100); // This is now handled inside line_curvature

    if (!result_ts) {
        std::cerr << "LineCurvatureOperation::execute: 'line_curvature' failed to produce a result." << std::endl;
        return {};
    }

    std::cout << "LineCurvatureOperation executed successfully." << std::endl;
    return result_ts;
}
