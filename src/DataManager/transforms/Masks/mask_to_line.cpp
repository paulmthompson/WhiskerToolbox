#include "mask_to_line.hpp"

#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"

#include "CoreGeometry/line_resampling.hpp"
#include "CoreGeometry/order_line.hpp"
#include "Masks/utils/skeletonize.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <armadillo>

#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>// for std::accumulate
#include <vector>


// Helper function to fit a polynomial to the given data
std::vector<double> fit_polynomial_to_points(Line2D const & points, int order) {
    if (points.size() <= static_cast<size_t>(order)) {
        return {};// Not enough data points
    }

    // Calculate t values using the helper function
    std::vector<double> t_values = compute_t_values(points);
    if (t_values.empty()) {
        return {};
    }

    // Extract x and y coordinates
    std::vector<double> x_coords;
    std::vector<double> y_coords;

    x_coords.reserve(points.size());
    y_coords.reserve(points.size());

    for (auto const & point: points) {
        x_coords.push_back(static_cast<double>(point.x));
        y_coords.push_back(static_cast<double>(point.y));
    }

    // Create Armadillo matrix for Vandermonde matrix
    arma::mat X(t_values.size(), order + 1);
    arma::vec Y(y_coords.data(), y_coords.size());

    // Build Vandermonde matrix
    for (size_t i = 0; i < t_values.size(); ++i) {
        for (int j = 0; j <= order; ++j) {
            X(i, j) = std::pow(t_values[i], j);
        }
    }

    // Solve least squares problem
    arma::vec coeffs;
    bool success = arma::solve(coeffs, X, Y);

    if (!success) {
        return {};// Failed to solve
    }

    // Convert Armadillo vector to std::vector
    std::vector<double> result(coeffs.begin(), coeffs.end());
    return result;
}

ParametricCoefficients fit_parametric_polynomials(Line2D const & points, int order) {
    if (points.size() <= static_cast<size_t>(order)) {
        return {{}, {}, false};// Not enough points
    }

    std::vector<double> t_values = compute_t_values(points);
    if (t_values.empty()) {
        return {{}, {}, false};
    }

    std::vector<double> x_coords;
    std::vector<double> y_coords;
    x_coords.reserve(points.size());
    y_coords.reserve(points.size());

    for (auto const & point: points) {
        x_coords.push_back(static_cast<double>(point.x));
        y_coords.push_back(static_cast<double>(point.y));
    }

    std::vector<double> x_coeffs = fit_single_dimension_polynomial_internal(x_coords, t_values, order);
    std::vector<double> y_coeffs = fit_single_dimension_polynomial_internal(y_coords, t_values, order);

    if (x_coeffs.empty() || y_coeffs.empty()) {
        return {{}, {}, false};
    }

    return {x_coeffs, y_coeffs, true};
}

// Helper function to generate a smoothed line from parametric polynomial coefficients
Line2D generate_smoothed_line(
        Line2D const & original_points,// Used to estimate total length
        std::vector<double> const & x_coeffs,
        std::vector<double> const & y_coeffs,
        int order,
        float target_spacing) {

    static_cast<void>(order);

    if (original_points.empty() || x_coeffs.empty() || y_coeffs.empty()) {
        return {};
    }

    // Estimate total arc length from original points for determining number of samples
    double total_length = 0.0;
    if (original_points.size() > 1) {
        for (size_t i = 1; i < original_points.size(); ++i) {
            double dx = original_points[i].x - original_points[i - 1].x;
            double dy = original_points[i].y - original_points[i - 1].y;
            total_length += std::sqrt(dx * dx + dy * dy);
        }
    }

    // If total length is very small or zero, or only one point, just return the (fitted) first point
    if (total_length < 1e-6 || original_points.size() <= 1 || target_spacing <= 1e-6) {
        if (!original_points.empty()) {
            // Evaluate polynomial at t=0 (or use the first point if no coeffs)
            if (!x_coeffs.empty() && !y_coeffs.empty()) {
                float x = static_cast<float>(evaluate_polynomial(x_coeffs, 0.0));
                float y = static_cast<float>(evaluate_polynomial(y_coeffs, 0.0));
                return Line2D({{x, y}});
            } else {
                return Line2D({original_points[0]});
            }
        } else {
            return {};
        }
    }

    // Determine number of samples based on target_spacing
    int num_samples = std::max(2, static_cast<int>(std::round(total_length / target_spacing)));

    std::vector<Point2D<float>> smoothed_line;
    smoothed_line.reserve(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(num_samples - 1);// t in [0,1]
        float x = static_cast<float>(evaluate_polynomial(x_coeffs, t));
        float y = static_cast<float>(evaluate_polynomial(y_coeffs, t));
        smoothed_line.push_back({x, y});
    }
    return smoothed_line;
}

// Calculate the error between fitted polynomial and original points
std::vector<float> calculate_fitting_errors(std::vector<Point2D<float>> const & points,
                                            std::vector<double> const & x_coeffs,
                                            std::vector<double> const & y_coeffs) {
    if (points.empty()) {
        return {};
    }

    // Calculate t values using the helper function
    std::vector<double> t_values = compute_t_values(points);
    if (t_values.empty()) {
        return {};
    }

    std::vector<float> errors;
    errors.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i) {
        double fitted_x = evaluate_polynomial(x_coeffs, t_values[i]);
        double fitted_y = evaluate_polynomial(y_coeffs, t_values[i]);

        // Use squared error directly (no square root)
        float error_squared = std::pow(points[i].x - fitted_x, 2) +
                              std::pow(points[i].y - fitted_y, 2);
        errors.push_back(error_squared);
    }

    return errors;
}

// Recursive helper function for removing outliers
Line2D remove_outliers_recursive(Line2D const & points,
                                 float error_threshold_squared,
                                 int polynomial_order,
                                 int max_iterations) {
    if (points.size() < static_cast<size_t>(polynomial_order + 2) || max_iterations <= 0) {
        return points;// Base case: not enough points or max iterations reached
    }

    // Calculate t values for parameterization
    std::vector<double> t_values = compute_t_values(points);
    if (t_values.empty()) {
        return points;
    }

    // Extract x and y coordinates
    std::vector<double> x_coords;
    std::vector<double> y_coords;

    x_coords.reserve(points.size());
    y_coords.reserve(points.size());

    for (auto const & point: points) {
        x_coords.push_back(static_cast<double>(point.x));
        y_coords.push_back(static_cast<double>(point.y));
    }

    // Create Armadillo matrices
    arma::mat X(t_values.size(), polynomial_order + 1);
    arma::vec X_vec(x_coords.data(), x_coords.size());
    arma::vec Y_vec(y_coords.data(), y_coords.size());

    // Build Vandermonde matrix
    for (size_t i = 0; i < t_values.size(); ++i) {
        for (int j = 0; j <= polynomial_order; ++j) {
            X(i, j) = std::pow(t_values[i], j);
        }
    }

    // Solve least squares problems
    arma::vec x_coeffs;
    arma::vec y_coeffs;
    bool success_x = arma::solve(x_coeffs, X, X_vec);
    bool success_y = arma::solve(y_coeffs, X, Y_vec);

    if (!success_x || !success_y) {
        return points;// Failed to fit, return original points
    }

    // Calculate errors and filter points
    std::vector<Point2D<float>> filtered_points;
    filtered_points.reserve(points.size());
    bool any_points_removed = false;

    for (size_t i = 0; i < points.size(); ++i) {
        double fitted_x = 0.0;
        double fitted_y = 0.0;

        for (int j = 0; j <= polynomial_order; ++j) {
            fitted_x += x_coeffs(j) * std::pow(t_values[i], j);
            fitted_y += y_coeffs(j) * std::pow(t_values[i], j);
        }

        // Use squared error directly (no square root)
        float error_squared = std::pow(points[i].x - fitted_x, 2) +
                              std::pow(points[i].y - fitted_y, 2);

        // Keep point if error is below threshold
        if (error_squared <= error_threshold_squared) {
            filtered_points.push_back(points[i]);
        } else {
            any_points_removed = true;
        }
    }

    // If we filtered too many points, return original set
    if (filtered_points.size() < static_cast<size_t>(polynomial_order + 2)) {
        return points;
    }

    // If we removed points, recursively call with filtered points
    if (any_points_removed) {
        return remove_outliers_recursive(filtered_points, error_threshold_squared, polynomial_order, max_iterations - 1);
    } else {
        return filtered_points;// No more points to remove
    }
}

// Main outlier removal function using recursion
Line2D remove_outliers(Line2D const & points,
                       float error_threshold,
                       int polynomial_order) {
    if (points.size() < static_cast<size_t>(polynomial_order + 2)) {
        return points;// Not enough points to fit and filter
    }

    // Convert threshold to squared threshold to avoid square roots in comparisons
    float error_threshold_squared = error_threshold * error_threshold;

    // Call recursive helper with a reasonable iteration limit
    return remove_outliers_recursive(points, error_threshold_squared, polynomial_order, 10);
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LineData> mask_to_line(MaskData const * mask_data, MaskToLineParameters const * params) {
    // Call the version with progress reporting but ignore progress
    return mask_to_line(mask_data, params, [](int) {});
}

std::shared_ptr<LineData> mask_to_line(MaskData const * mask_data,
                                       MaskToLineParameters const * params,
                                       ProgressCallback progressCallback) {

    auto line_map = std::map<TimeFrameIndex, std::vector<Line2D>>();

    // Use default parameters if none provided
    float reference_x = params ? params->reference_x : 0.0f;
    float reference_y = params ? params->reference_y : 0.0f;
    LinePointSelectionMethod method = params ? params->method : LinePointSelectionMethod::Skeletonize;
    int polynomial_order = params ? params->polynomial_order : 3;
    float error_threshold = params ? params->error_threshold : 5.0f;
    bool should_remove_outliers = params ? params->remove_outliers : true;
    int input_point_subsample_factor = params ? params->input_point_subsample_factor : 1;
    bool should_smooth_line = params ? params->should_smooth_line : false;
    float output_resolution = params ? params->output_resolution : 5.0f;

    std::cout << "reference_x: " << reference_x << std::endl;
    std::cout << "reference_y: " << reference_y << std::endl;
    std::cout << "method: " << static_cast<int>(method) << std::endl;
    std::cout << "polynomial_order: " << polynomial_order << std::endl;
    std::cout << "error_threshold: " << error_threshold << std::endl;
    std::cout << "should_remove_outliers: " << should_remove_outliers << std::endl;
    std::cout << "input_point_subsample_factor: " << input_point_subsample_factor << std::endl;
    std::cout << "should_smooth_line: " << should_smooth_line << std::endl;
    std::cout << "output_resolution: " << output_resolution << std::endl;

    Point2D<float> reference_point{reference_x, reference_y};

    // Initial progress
    progressCallback(0);

    // Count total masks to process for progress calculation
    size_t const total_masks = mask_data->size();

    if (total_masks == 0) {
        progressCallback(100);
        return std::make_shared<LineData>();// Nothing to process
    }

    // Create a binary image from the mask points
    ImageSize image_size = mask_data->getImageSize();
    if (image_size.width <= 0 || image_size.height <= 0) {
        // Use default size if mask data doesn't specify
        image_size.width = 256;
        image_size.height = 256;
    }

    std::vector<uint8_t> binary_image(image_size.width * image_size.height, 0);

    // Timing variables
    std::vector<long long> skeletonize_times;
    std::vector<long long> order_line_times;
    std::vector<long long> outlier_removal_times;
    std::vector<long long> map_insertion_times;
    std::vector<long long> smoothing_times;

    size_t processed_masks = 0;
    for (auto const & mask_time_pair: mask_data->getAllAsRange()) {
        auto time = mask_time_pair.time;
        auto const & masks = mask_time_pair.masks;

        if (masks.empty()) {
            continue;
        }

        // For now, just process the first mask at each time
        auto const & mask = masks[0];

        if (mask.empty()) {
            continue;
        }

        Line2D line_points;

        if (method == LinePointSelectionMethod::Skeletonize) {
            // Zero out the binary image
            std::fill(binary_image.begin(), binary_image.end(), 0);

            for (auto const & point: mask) {
                int x = static_cast<int>(point.x);
                int y = static_cast<int>(point.y);

                if (x >= 0 && x < image_size.width &&
                    y >= 0 && y < image_size.height) {
                    binary_image[y * image_size.width + x] = 1;
                }
            }

            // Time skeletonization
            auto skeletonize_start = std::chrono::high_resolution_clock::now();
            auto skeleton = fast_skeletonize(binary_image, image_size.height, image_size.width);
            auto skeletonize_end = std::chrono::high_resolution_clock::now();
            skeletonize_times.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            skeletonize_end - skeletonize_start)
                            .count());

            // Time ordering step
            auto order_start = std::chrono::high_resolution_clock::now();
            line_points = order_line(skeleton, image_size, reference_point, input_point_subsample_factor);
            auto order_end = std::chrono::high_resolution_clock::now();
            order_line_times.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            order_end - order_start)
                            .count());
        } else {

            // Time ordering step for this method
            auto order_start = std::chrono::high_resolution_clock::now();
            line_points = order_line(mask, reference_point, input_point_subsample_factor);
            auto order_end = std::chrono::high_resolution_clock::now();
            order_line_times.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            order_end - order_start)
                            .count());
        }

        if (should_remove_outliers && line_points.size() > static_cast<size_t>(polynomial_order + 2)) {
            // Time outlier removal
            auto outlier_start = std::chrono::high_resolution_clock::now();
            line_points = remove_outliers(line_points, error_threshold, polynomial_order);
            auto outlier_end = std::chrono::high_resolution_clock::now();
            outlier_removal_times.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            outlier_end - outlier_start)
                            .count());
        }

        // Apply smoothing if requested and enough points exist
        if (should_smooth_line && line_points.size() > static_cast<size_t>(polynomial_order)) {// Need at least order+1 points
            auto smoothing_start = std::chrono::high_resolution_clock::now();
            ParametricCoefficients coeffs = fit_parametric_polynomials(line_points, polynomial_order);
            if (coeffs.success) {
                line_points = generate_smoothed_line(line_points, coeffs.x_coeffs, coeffs.y_coeffs, polynomial_order, output_resolution);
            } else {
                // Smoothing failed, fall back to resampling the existing (ordered, possibly outlier-removed) points
                if (!line_points.empty()) {
                    line_points = resample_line_points(line_points, output_resolution);
                }
            }
            auto smoothing_end = std::chrono::high_resolution_clock::now();
            smoothing_times.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            smoothing_end - smoothing_start)
                            .count());
        } else if (!line_points.empty()) {
            // If not smoothing (or not enough points for smoothing), apply resampling directly
            line_points = resample_line_points(line_points, output_resolution);
        }

        if (!line_points.empty()) {
            // Time map insertion
            auto insertion_start = std::chrono::high_resolution_clock::now();
            line_map[time].push_back(std::move(line_points));
            auto insertion_end = std::chrono::high_resolution_clock::now();
            map_insertion_times.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            insertion_end - insertion_start)
                            .count());
        }

        processed_masks++;

        // Print timing statistics every 1000 iterations or on the last iteration
        if (processed_masks % 1000 == 0 || processed_masks == total_masks) {
            if (!skeletonize_times.empty()) {
                double skeletonize_avg = std::accumulate(skeletonize_times.begin(), skeletonize_times.end(), 0.0) /
                                         skeletonize_times.size();
                std::cout << "Average skeletonization time: " << skeletonize_avg << " μs" << std::endl;
            }

            if (!order_line_times.empty()) {
                double order_avg = std::accumulate(order_line_times.begin(), order_line_times.end(), 0.0) /
                                   order_line_times.size();
                std::cout << "Average order_line time: " << order_avg << " μs" << std::endl;
            }

            if (!outlier_removal_times.empty()) {
                double outlier_avg = std::accumulate(outlier_removal_times.begin(), outlier_removal_times.end(), 0.0) /
                                     outlier_removal_times.size();
                std::cout << "Average outlier removal time: " << outlier_avg << " μs" << std::endl;
            }

            if (!map_insertion_times.empty()) {
                double insertion_avg = std::accumulate(map_insertion_times.begin(), map_insertion_times.end(), 0.0) /
                                       map_insertion_times.size();
                std::cout << "Average map insertion time: " << insertion_avg << " μs" << std::endl;
            }

            if (!smoothing_times.empty()) {
                double smoothing_avg = std::accumulate(smoothing_times.begin(), smoothing_times.end(), 0.0) /
                                       smoothing_times.size();
                std::cout << "Average smoothing time: " << smoothing_avg << " μs" << std::endl;
            }

            // Clear the vectors to only keep the last 1000 measurements
            if (processed_masks % 1000 == 0 && processed_masks < total_masks) {
                skeletonize_times.clear();
                order_line_times.clear();
                outlier_removal_times.clear();
                map_insertion_times.clear();
                smoothing_times.clear();
            }
        }

        int progress = static_cast<int>(std::round((static_cast<double>(processed_masks) / total_masks) * 100.0));
        progressCallback(progress);
    }

    auto line_data = std::make_shared<LineData>(line_map);

    // Copy the image size from mask data to line data
    line_data->setImageSize(mask_data->getImageSize());

    progressCallback(100);

    return line_data;
}

///////////////////////////////////////////////////////////////////////////////// 

std::string MaskToLineOperation::getName() const {
    return "Convert Mask to Line";
}

std::type_index MaskToLineOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskToLineOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<MaskData>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> MaskToLineOperation::getDefaultParameters() const {
    return std::make_unique<MaskToLineParameters>();
}

DataTypeVariant MaskToLineOperation::execute(DataTypeVariant const & dataVariant,
                                             TransformParametersBase const * transformParameters) {
    // Call the version with progress reporting but ignore progress
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant MaskToLineOperation::execute(DataTypeVariant const & dataVariant,
                                             TransformParametersBase const * transformParameters,
                                             ProgressCallback progressCallback) {
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "MaskToLineOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty variant
    }

    MaskData const * mask_raw_ptr = (*ptr_ptr).get();

    MaskToLineParameters const * typed_params = nullptr;
    if (transformParameters) {
        typed_params = dynamic_cast<MaskToLineParameters const *>(transformParameters);
        if (!typed_params) {
            std::cerr << "MaskToLineOperation::execute: Invalid parameter type" << std::endl;
        }
    }

    std::shared_ptr<LineData> result_line = mask_to_line(mask_raw_ptr, typed_params, progressCallback);

    // Handle potential failure from the calculation function
    if (!result_line) {
        std::cerr << "MaskToLineOperation::execute: 'mask_to_line' failed to produce a result." << std::endl;
        return {};// Return empty variant
    }

    std::cout << "MaskToLineOperation executed successfully." << std::endl;
    return result_line;
}
