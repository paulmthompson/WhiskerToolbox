#include "line_angle.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "CoreGeometry/angle.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "transforms/utils/variant_type_check.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <armadillo>

#include <cmath>
#include <map>
#include <numbers>
#include <vector>


std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data, LineAngleParameters const * params) {
    // Call the version with progress reporting but ignore progress
    return line_angle(line_data, params, [](int) {});
}

std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data,
                                             LineAngleParameters const * params,
                                             ProgressCallback progressCallback) {

    static_cast<void>(progressCallback);

    std::map<int, float> angles;

    // Use default parameters if none provided
    float position = params ? params->position : 0.2f;
    AngleCalculationMethod method = params ? params->method : AngleCalculationMethod::DirectPoints;
    int polynomial_order = params ? params->polynomial_order : 3;
    float reference_x = params ? params->reference_x : 1.0f;
    float reference_y = params ? params->reference_y : 0.0f;

    // Ensure position is within valid range
    position = std::max(0.0f, std::min(position, 1.0f));

    // Normalize reference vector if needed
    if (reference_x != 0.0f || reference_y != 0.0f) {
        float length = std::sqrt(reference_x * reference_x + reference_y * reference_y);
        if (length > 0.0f) {
            reference_x /= length;
            reference_y /= length;
        } else {
            // Default to x-axis if invalid reference
            reference_x = 1.0f;
            reference_y = 0.0f;
        }
    } else {
        // Default to x-axis if zero reference
        reference_x = 1.0f;
        reference_y = 0.0f;
    }

    for (auto const & [time, entity_id, line]: line_data->flattened_data()) {

        if (line.size() < 2) {
            continue;
        }

        float angle = 0.0f;

        // Calculate angle using the selected method
        if (method == AngleCalculationMethod::DirectPoints) {
            angle = calculate_direct_angle(line, position, reference_x, reference_y);
        } else if (method == AngleCalculationMethod::PolynomialFit) {
            angle = calculate_polynomial_angle(line, position, polynomial_order, reference_x, reference_y);
        }

        angles[static_cast<int>(time.getValue())] = angle;
    }

    return std::make_shared<AnalogTimeSeries>(angles);
}

///////////////////////////////////////////////////////////////////////////////

std::string LineAngleOperation::getName() const {
    return "Calculate Line Angle";
}

std::type_index LineAngleOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineAngleOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineAngleOperation::getDefaultParameters() const {
    return std::make_unique<LineAngleParameters>();
}

DataTypeVariant LineAngleOperation::execute(DataTypeVariant const & dataVariant,
                                            TransformParametersBase const * transformParameters,
                                            ProgressCallback progressCallback) {
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "LineAngleOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty variant
    }

    LineData const * line_raw_ptr = (*ptr_ptr).get();

    LineAngleParameters const * typed_params = nullptr;
    if (transformParameters) {
        typed_params = dynamic_cast<LineAngleParameters const *>(transformParameters);
        if (!typed_params) {
            std::cerr << "LineAngleOperation::execute: Invalid parameter type" << std::endl;
        }
    }

    std::shared_ptr<AnalogTimeSeries> result_ts = line_angle(line_raw_ptr, typed_params, progressCallback);

    // Handle potential failure from the calculation function
    if (!result_ts) {
        std::cerr << "LineAngleOperation::execute: 'line_angle' failed to produce a result." << std::endl;
        return {};// Return empty variant
    }

    std::cout << "LineAngleOperation executed successfully using variant input." << std::endl;
    return result_ts;
}

DataTypeVariant LineAngleOperation::execute(DataTypeVariant const & dataVariant,
                                            TransformParametersBase const * transformParameters) {

    return execute(dataVariant, transformParameters, [](int){});
}
