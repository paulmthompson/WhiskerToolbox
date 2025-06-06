#include "line_angle.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Lines/utils/line_geometry.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <armadillo>

#include <cmath>
#include <map>
#include <numbers>
#include <vector>


float normalize_angle(float raw_angle, float reference_x, float reference_y) {
    // Calculate the angle of the reference vector (if not the default x-axis)
    float reference_angle = 0.0f;
    if (!(reference_x == 1.0f && reference_y == 0.0f)) {
        reference_angle = std::atan2(reference_y, reference_x);
        // Convert to degrees
        reference_angle *= 180.0f / static_cast<float>(std::numbers::pi);
    }

    // Adjust the raw angle by subtracting the reference angle
    float normalized_angle = raw_angle - reference_angle;

    // Normalize to range [-180, 180]
    while (normalized_angle > 180.0f) normalized_angle -= 360.0f;
    while (normalized_angle <= -180.0f) normalized_angle += 360.0f;

    return normalized_angle;
}

// Calculate angle using direct point comparison
float calculate_direct_angle(Line2D const & line, float position, float reference_x, float reference_y) {
    if (line.size() < 2) {
        return 0.0f;
    }

    // Calculate the index of the position point
    auto idx = static_cast<size_t>(position * static_cast<float>((line.size() - 1)));
    if (idx == 0) {
        // If position is at the start, use the first two points
        idx = 1;
    } else if (idx >= line.size() - 1) {
        // If position is at the end, use the last two points
        idx = line.size() - 2;
    }
    if (idx >= line.size()) {
        idx = line.size() - 1;
    }

    Point2D<float> const base = line[0];
    Point2D<float> const pos = line[idx];

    // Calculate angle in radians (atan2 returns value in range [-π, π])
    float raw_angle = std::atan2(pos.y - base.y, pos.x - base.x);

    // Convert to degrees
    float angle_degrees = raw_angle * 180.0f / static_cast<float>(std::numbers::pi);

    // Normalize with respect to the reference vector
    return normalize_angle(angle_degrees, reference_x, reference_y);
}

// Calculate angle using polynomial parameterization
float calculate_polynomial_angle(Line2D const & line, float position, int polynomial_order,
                                 float reference_x, float reference_y) {
    if (line.size() < polynomial_order + 1) {
        // Fall back to direct method if not enough points
        return calculate_direct_angle(line, position, reference_x, reference_y);
    }

    // Extract x and y coordinates
    std::vector<double> x_coords;
    std::vector<double> y_coords;

    auto length = calc_length(line);

    x_coords.reserve(line.size());
    y_coords.reserve(line.size());
    auto t_values_f = calc_cumulative_length_vector(line);
    for (size_t i = 0; i < line.size(); ++i) {
        // Normalize t_values to [0, 1]
        t_values_f[i] /= length;
    }

    std::vector<double> t_values(t_values_f.begin(), t_values_f.end());

    for (size_t i = 0; i < line.size(); ++i) {
        x_coords.push_back(static_cast<double>(line[i].x));
        y_coords.push_back(static_cast<double>(line[i].y));
    }

    // Fit polynomials to x(t) and y(t)
    std::vector<double> x_coeffs = fit_polynomial(t_values, x_coords, polynomial_order);
    std::vector<double> y_coeffs = fit_polynomial(t_values, y_coords, polynomial_order);

    if (x_coeffs.empty() || y_coeffs.empty()) {
        // Fall back to direct method if fitting failed
        return calculate_direct_angle(line, position, reference_x, reference_y);
    }

    // Evaluate derivatives at the specified position
    double t = static_cast<double>(position);
    double dx_dt = evaluate_polynomial_derivative(x_coeffs, t);
    double dy_dt = evaluate_polynomial_derivative(y_coeffs, t);

    // Calculate angle using the derivatives
    double raw_angle = std::atan2(dy_dt, dx_dt);

    // Convert to degrees
    float angle_degrees = static_cast<float>(raw_angle * 180.0 / std::numbers::pi);

    // Normalize with respect to the reference vector
    return normalize_angle(angle_degrees, reference_x, reference_y);
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data, LineAngleParameters const * params) {
    // Call the version with progress reporting but ignore progress
    return line_angle(line_data, params, [](int) {});
}

std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data,
                                             LineAngleParameters const * params,
                                             ProgressCallback progressCallback) {
    auto analog_time_series = std::make_shared<AnalogTimeSeries>();
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

    for (auto const & line_and_time: line_data->GetAllLinesAsRange()) {
        if (line_and_time.lines.empty()) {
            continue;
        }

        Line2D const & line = line_and_time.lines[0];

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

        angles[line_and_time.time] = angle;
    }

    analog_time_series->setData(angles);
    return analog_time_series;
}

///////////////////////////////////////////////////////////////////////////////

std::string LineAngleOperation::getName() const {
    return "Calculate Line Angle";
}

std::type_index LineAngleOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineAngleOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);

    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> LineAngleOperation::getDefaultParameters() const {
    return std::make_unique<LineAngleParameters>();
}

DataTypeVariant LineAngleOperation::execute(DataTypeVariant const & dataVariant,
                                            TransformParametersBase const * transformParameters) {

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

    std::shared_ptr<AnalogTimeSeries> result_ts = line_angle(line_raw_ptr, typed_params);

    // Handle potential failure from the calculation function
    if (!result_ts) {
        std::cerr << "LineAngleOperation::execute: 'line_angle' failed to produce a result." << std::endl;
        return {};// Return empty variant
    }

    std::cout << "LineAngleOperation executed successfully using variant input." << std::endl;
    return result_ts;
}
