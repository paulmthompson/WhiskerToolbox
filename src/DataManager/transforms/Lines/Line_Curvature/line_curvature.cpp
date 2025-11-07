#include "line_curvature.hpp"

#include "transforms/utils/variant_type_check.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <cmath>   // std::sqrt, std::pow, std::abs, NAN
#include <iostream>// For std::cerr
#include <map>     // For std::map
#include <vector>  // For std::vector


std::optional<float> calculate_polynomial_curvature(
        Line2D const & line,
        float t_position,// Fractional position 0.0 to 1.0
        int polynomial_order,
        float fitting_window_percentage) {// Re-added fitting_window_percentage

    if (line.size() < static_cast<size_t>(polynomial_order + 1) || line.size() < 2) {
        std::cerr << "LineCurvature: Not enough points for polynomial fit. Need at least "
                  << polynomial_order + 1 << ", got " << line.size() << std::endl;
        return std::nullopt;
    }

    std::vector<double> t_values_full_line = compute_t_values(line);
    if (t_values_full_line.empty()) {
        std::cerr << "LineCurvature: Failed to compute t-values for the full line." << std::endl;
        return std::nullopt;
    }

    std::vector<double> x_coords, y_coords;
    x_coords.reserve(line.size());
    y_coords.reserve(line.size());
    for (auto const & p: line) {
        x_coords.push_back(static_cast<double>(p.x));
        y_coords.push_back(static_cast<double>(p.y));
    }

    std::vector<double> x_coeffs = fit_single_dimension_polynomial_internal(x_coords, t_values_full_line, polynomial_order);
    std::vector<double> y_coeffs = fit_single_dimension_polynomial_internal(y_coords, t_values_full_line, polynomial_order);

    if (x_coeffs.empty() || y_coeffs.empty()) {
        std::cerr << "LineCurvature: Polynomial fitting failed for X or Y coefficients on the full line." << std::endl;
        return std::nullopt;
    }

    // t_eval is the point at which we want to calculate curvature, relative to the [0,1] of the whole line.
    double t_eval = std::max(0.0, std::min(1.0, static_cast<double>(t_position)));

    // h is half the fitting_window_percentage, as it's the step from t_eval to t_eval+h or t_eval-h
    // Ensure fitting_window_percentage is positive and not excessively large.
    fitting_window_percentage = std::max(0.001f, std::min(1.0f, fitting_window_percentage));// e.g. min 0.1% window, max 100%
    double h = static_cast<double>(fitting_window_percentage) / 2.0;

    if (h == 0.0) {
        std::cerr << "LineCurvature: fitting_window_percentage results in h=0, cannot use central differences." << std::endl;
        return 0.0f;// Or std::nullopt, as curvature is ill-defined here
    }

    // Points for central differences, clamped to [0,1] interval of the line's t-parameterization
    double t_minus_h = std::max(0.0, t_eval - h);
    double t_plus_h = std::min(1.0, t_eval + h);

    // Evaluate polynomial at t_eval, t_eval-h, and t_eval+h
    double x_t = evaluate_polynomial(x_coeffs, t_eval);
    double y_t = evaluate_polynomial(y_coeffs, t_eval);
    double x_t_minus_h = evaluate_polynomial(x_coeffs, t_minus_h);
    double y_t_minus_h = evaluate_polynomial(y_coeffs, t_minus_h);
    double x_t_plus_h = evaluate_polynomial(x_coeffs, t_plus_h);
    double y_t_plus_h = evaluate_polynomial(y_coeffs, t_plus_h);

    // Calculate derivatives using central differences
    // Note: The effective h for points that hit the boundary (0 or 1) will be smaller.
    // We should use the actual distance between evaluated points.
    //double h_actual_fwd = t_plus_h - t_eval;
    //double h_actual_bwd = t_eval - t_minus_h;

    // If t_eval is at 0 or 1, central difference for 1st derivative is not ideal.
    // However, for curvature we need x_prime, y_prime, x_double_prime, y_double_prime all at t_eval.
    // The standard central difference formulas assume symmetric points around t_eval.
    // If t_plus_h or t_minus_h were clamped, the symmetry is broken.
    // A robust way for general h: use the formulas as is, but with potentially asymmetric actual h values is complex.
    // For simplicity, let's stick to the formulas with the intended h, recognizing boundary effects.
    // An alternative for boundaries would be to switch to forward/backward differences, but that complicates things.

    double x_prime, y_prime, x_double_prime, y_double_prime;

    // First derivatives: (f(t+h) - f(t-h)) / (2h)
    // Ensure 2*h is not zero. This should be covered by h > 0 check earlier.
    if ((t_plus_h - t_minus_h) < 1e-9) {// if t_plus_h and t_minus_h are effectively the same point
        x_prime = 0;
        y_prime = 0;// No change, derivative is zero
    } else {
        x_prime = (x_t_plus_h - x_t_minus_h) / (t_plus_h - t_minus_h);// Denominator is (t+h) - (t-h) = 2h if not clamped
        y_prime = (y_t_plus_h - y_t_minus_h) / (t_plus_h - t_minus_h);
    }

    // Second derivatives: (f(t+h) - 2f(t) + f(t-h)) / h^2
    // This requires a bit more care if h_actual_fwd and h_actual_bwd are different due to clamping.
    // The general formula for non-uniform spacing is more complex.
    // For simplicity, we'll use the standard formula with the original h, acknowledging potential inaccuracies at boundaries.
    // Or, if h_actual_fwd and h_actual_bwd are very different, it might be problematic.
    // A simple approach is to average h_actual_fwd and h_actual_bwd if they are different.
    double h_for_ddot_sq = h * h;
    if (h_for_ddot_sq < 1e-9) {
        x_double_prime = 0;
        y_double_prime = 0;
    } else {
        x_double_prime = (x_t_plus_h - 2.0 * x_t + x_t_minus_h) / h_for_ddot_sq;
        y_double_prime = (y_t_plus_h - 2.0 * y_t + y_t_minus_h) / h_for_ddot_sq;
    }

    double numerator = x_prime * y_double_prime - y_prime * x_double_prime;
    double denominator_sq_term = x_prime * x_prime + y_prime * y_prime;

    if (std::abs(denominator_sq_term) < 1e-9) {// Avoid division by zero if speed is zero (e.g., single point or error)
        // This typically means the point is stationary or the parameterization is degenerate.
        // For a truly straight line, curvature is 0, but this case is more about numerical stability.
        return 0.0f;// Or std::nullopt if zero is not appropriate for degenerate cases
    }

    double curvature = numerator / std::pow(denominator_sq_term, 1.5);

    if (std::isnan(curvature) || std::isinf(curvature)) {
        std::cerr << "LineCurvature: Calculated curvature is NaN or Inf." << std::endl;
        return std::nullopt;
    }

    return static_cast<float>(curvature);
}

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
        progressCallback(100);    // Still call progress to complete
        return std::make_shared<AnalogTimeSeries>();// Return empty series
    }

    // Determine total number of time points for progress calculation
    size_t total_time_points = 0;
    for (auto const & [time, entries]: line_data->getAllEntries()) {
        total_time_points++;
    }
    if (total_time_points == 0) {
        progressCallback(100);
        return std::make_shared<AnalogTimeSeries>();
    }

    size_t processed_time_points = 0;
    progressCallback(0);// Initial progress

    float position = params->position;
    int polynomial_order = params->polynomial_order;
    float fitting_window = params->fitting_window_percentage;// Get fitting_window again

    for (auto const & [time, entries]: line_data->getAllEntries()) {
        if (entries.empty()) {
            processed_time_points++;
            int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * 100.0));
            progressCallback(current_progress);
            continue;
        }

        // Process only the first line at each time point, similar to LineAngle
        Line2D const & line = entries[0].data;

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
