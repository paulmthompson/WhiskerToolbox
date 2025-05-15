#include "line_angle.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"

#include <armadillo>

#include <cmath>
#include <map>
#include <numbers>
#include <vector>


// Helper function to fit a polynomial of the specified order to the given data
std::vector<double> fit_polynomial(std::vector<double> const &x, std::vector<double> const &y, int order) {
    if (x.size() != y.size() || x.size() <= order) {
        return {};  // Not enough data points or size mismatch
    }

    // Create Armadillo matrix for Vandermonde matrix
    arma::mat X(x.size(), order + 1);
    arma::vec Y(y.data(), y.size());

    // Build Vandermonde matrix
    for (size_t i = 0; i < x.size(); ++i) {
        for (int j = 0; j <= order; ++j) {
            X(i, j) = std::pow(x[i], j);
        }
    }

    // Solve least squares problem: X * coeffs = Y
    arma::vec coeffs;
    bool success = arma::solve(coeffs, X, Y);
    
    if (!success) {
        return {}; // Failed to solve
    }

    // Convert Armadillo vector to std::vector
    std::vector<double> result(coeffs.begin(), coeffs.end());
    return result;
}

// Helper function to evaluate polynomial derivative at a given point
double evaluate_polynomial_derivative(std::vector<double> const &coeffs, double x) {
    double result = 0.0;
    for (size_t i = 1; i < coeffs.size(); ++i) {
        result += i * coeffs[i] * std::pow(x, i - 1);
    }
    return result;
}

// Calculate angle using direct point comparison
float calculate_direct_angle(Line2D const &line, float position) {
    if (line.size() < 2) {
        return 0.0f;
    }
    
    // Calculate the index of the position point
    auto idx = static_cast<size_t>(position * static_cast<float>((line.size() - 1)));
    if (idx >= line.size()) {
        idx = line.size() - 1;
    }
    
    Point2D<float> const base = line[0];
    Point2D<float> const pos = line[idx];
    
    // Calculate angle in radians (atan2 returns value in range [-π, π])
    float angle = std::atan2(pos.y - base.y, pos.x - base.x);
    
    // Convert to degrees and return
    return angle * 180.0f / static_cast<float>(std::numbers::pi);
}

// Calculate angle using polynomial parameterization
float calculate_polynomial_angle(Line2D const &line, float position, int polynomial_order) {
    if (line.size() < polynomial_order + 1) {
        // Fall back to direct method if not enough points
        return calculate_direct_angle(line, position);
    }

    // Extract x and y coordinates
    std::vector<double> x_coords;
    std::vector<double> y_coords;
    std::vector<double> t_values;  // Parameter values (0 to 1)
    
    x_coords.reserve(line.size());
    y_coords.reserve(line.size());
    t_values.reserve(line.size());
    
    for (size_t i = 0; i < line.size(); ++i) {
        x_coords.push_back(static_cast<double>(line[i].x));
        y_coords.push_back(static_cast<double>(line[i].y));
        t_values.push_back(static_cast<double>(i) / static_cast<double>(line.size() - 1));
    }
    
    // Fit polynomials to x(t) and y(t)
    std::vector<double> x_coeffs = fit_polynomial(t_values, x_coords, polynomial_order);
    std::vector<double> y_coeffs = fit_polynomial(t_values, y_coords, polynomial_order);
    
    if (x_coeffs.empty() || y_coeffs.empty()) {
        // Fall back to direct method if fitting failed
        return calculate_direct_angle(line, position);
    }
    
    // Evaluate derivatives at the specified position
    double t = static_cast<double>(position);
    double dx_dt = evaluate_polynomial_derivative(x_coeffs, t);
    double dy_dt = evaluate_polynomial_derivative(y_coeffs, t);
    
    // Calculate angle using the derivatives
    double angle = std::atan2(dy_dt, dx_dt);
    
    // Convert to degrees and return
    return static_cast<float>(angle * 180.0 / std::numbers::pi);
}

std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data, LineAngleParameters const * params) {
    auto analog_time_series = std::make_shared<AnalogTimeSeries>();
    std::map<int, float> angles;
    
    // Use default parameters if none provided
    float position = params ? params->position : 0.2f;
    AngleCalculationMethod method = params ? params->method : AngleCalculationMethod::DirectPoints;
    int polynomial_order = params ? params->polynomial_order : 3;
    
    // Ensure position is within valid range
    position = std::max(0.0f, std::min(position, 1.0f));
    
    for (auto const & line_and_time : line_data->GetAllLinesAsRange()) {
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
            angle = calculate_direct_angle(line, position);
        } else if (method == AngleCalculationMethod::PolynomialFit) {
            angle = calculate_polynomial_angle(line, position, polynomial_order);
        }
        
        angles[line_and_time.time] = angle;
    }
    
    analog_time_series->setData(angles);
    return analog_time_series;
}

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
        return {};  // Return empty variant
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
        return {};  // Return empty variant
    }
    
    std::cout << "LineAngleOperation executed successfully using variant input." << std::endl;
    return result_ts;
}
