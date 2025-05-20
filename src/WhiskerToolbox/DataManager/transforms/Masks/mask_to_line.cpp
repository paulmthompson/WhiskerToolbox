#include "mask_to_line.hpp"

#include "Masks/Mask_Data.hpp"
#include "Lines/Line_Data.hpp"

#include "mask_operations.hpp"
#include "order_line.hpp"
#include "simplify_line.hpp"
#include "skeletonize.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <armadillo>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

// Helper function to fit a polynomial to the given data
std::vector<double> fit_polynomial_to_points(const std::vector<Point2D<float>>& points, int order) {
    if (points.size() <= order) {
        return {};  // Not enough data points
    }

    // Extract x and y coordinates
    std::vector<double> x_coords;
    std::vector<double> y_coords;
    std::vector<double> t_values;  // Parameter values (0 to 1)
    
    x_coords.reserve(points.size());
    y_coords.reserve(points.size());
    t_values.reserve(points.size());
    
    for (size_t i = 0; i < points.size(); ++i) {
        x_coords.push_back(static_cast<double>(points[i].x));
        y_coords.push_back(static_cast<double>(points[i].y));
        t_values.push_back(static_cast<double>(i) / static_cast<double>(points.size() - 1));
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
        return {}; // Failed to solve
    }

    // Convert Armadillo vector to std::vector
    std::vector<double> result(coeffs.begin(), coeffs.end());
    return result;
}

// Calculate the error between fitted polynomial and original points
std::vector<float> calculate_fitting_errors(const std::vector<Point2D<float>>& points, 
                                          const std::vector<double>& x_coeffs, 
                                          const std::vector<double>& y_coeffs) {
    std::vector<float> errors;
    errors.reserve(points.size());
    
    for (size_t i = 0; i < points.size(); ++i) {
        double t = static_cast<double>(i) / static_cast<double>(points.size() - 1);
        double fitted_x = evaluate_polynomial(x_coeffs, t);
        double fitted_y = evaluate_polynomial(y_coeffs, t);
        
        float error = std::sqrt(std::pow(points[i].x - fitted_x, 2) + 
                              std::pow(points[i].y - fitted_y, 2));
        errors.push_back(error);
    }
    
    return errors;
}

// Remove outliers from points based on error threshold
std::vector<Point2D<float>> remove_outliers(const std::vector<Point2D<float>>& points, 
                                          float error_threshold, 
                                          int polynomial_order) {
    if (points.size() < polynomial_order + 2) {
        return points;  // Not enough points to fit and filter
    }
    
    std::vector<Point2D<float>> filtered_points = points;
    bool points_removed = false;
    
    do {
        points_removed = false;
        
        // Fit polynomials to current points
        auto x_coords = std::vector<double>();
        auto y_coords = std::vector<double>();
        auto t_values = std::vector<double>();
        
        x_coords.reserve(filtered_points.size());
        y_coords.reserve(filtered_points.size());
        t_values.reserve(filtered_points.size());
        
        for (size_t i = 0; i < filtered_points.size(); ++i) {
            x_coords.push_back(static_cast<double>(filtered_points[i].x));
            y_coords.push_back(static_cast<double>(filtered_points[i].y));
            t_values.push_back(static_cast<double>(i) / static_cast<double>(filtered_points.size() - 1));
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
            break;  // Failed to fit
        }
        
        // Calculate errors and find worst offender
        float max_error = 0.0f;
        size_t worst_idx = 0;
        
        for (size_t i = 0; i < filtered_points.size(); ++i) {
            double t = static_cast<double>(i) / static_cast<double>(filtered_points.size() - 1);
            double fitted_x = 0.0;
            double fitted_y = 0.0;
            
            for (int j = 0; j <= polynomial_order; ++j) {
                fitted_x += x_coeffs(j) * std::pow(t, j);
                fitted_y += y_coeffs(j) * std::pow(t, j);
            }
            
            float error = std::sqrt(std::pow(filtered_points[i].x - fitted_x, 2) + 
                                  std::pow(filtered_points[i].y - fitted_y, 2));
            
            if (error > max_error) {
                max_error = error;
                worst_idx = i;
            }
        }
        
        // Remove point if error exceeds threshold
        if (max_error > error_threshold && filtered_points.size() > polynomial_order + 2) {
            filtered_points.erase(filtered_points.begin() + worst_idx);
            points_removed = true;
        }
        
    } while (points_removed);
    
    return filtered_points;
}

std::shared_ptr<LineData> mask_to_line(MaskData const* mask_data, MaskToLineParameters const* params) {
    // Call the version with progress reporting but ignore progress
    return mask_to_line(mask_data, params, [](int) {});
}

std::shared_ptr<LineData> mask_to_line(MaskData const* mask_data, 
                                       MaskToLineParameters const* params,
                                       ProgressCallback progressCallback) {
    auto line_data = std::make_shared<LineData>();
    
    // Use default parameters if none provided
    float reference_x = params ? params->reference_x : 0.0f;
    float reference_y = params ? params->reference_y : 0.0f;
    LinePointSelectionMethod method = params ? params->method : LinePointSelectionMethod::Skeletonize;
    int polynomial_order = params ? params->polynomial_order : 3;
    float error_threshold = params ? params->error_threshold : 5.0f;
    bool should_remove_outliers = params ? params->remove_outliers : true;
    int subsample = params ? params->subsample : 1;
    
    Point2D<float> reference_point{reference_x, reference_y};
    
    // Initial progress
    progressCallback(0);
    
    // Copy the image size from mask data to line data
    line_data->setImageSize(mask_data->getImageSize());
    
    // Count total masks to process for progress calculation
    size_t total_masks = 0;
    for (auto const& mask_time_pair : mask_data->getAllMasksAsRange()) {
        if (!mask_time_pair.masks.empty() && !mask_time_pair.masks[0].empty()) {
            total_masks++;
        }
    }
    
    if (total_masks == 0) {
        progressCallback(100);
        return line_data;  // Nothing to process
    }
    
    // Process each mask time point
    size_t processed_masks = 0;
    for (auto const& mask_time_pair : mask_data->getAllMasksAsRange()) {
        int time = mask_time_pair.time;
        auto const& masks = mask_time_pair.masks;
        
        if (masks.empty()) {
            continue;
        }
        
        // For now, just process the first mask at each time
        auto const& mask = masks[0];
        
        if (mask.empty()) {
            continue;
        }
        
        // Create a binary image from the mask points
        ImageSize image_size = mask_data->getImageSize();
        if (image_size.width <= 0 || image_size.height <= 0) {
            // Use default size if mask data doesn't specify
            image_size.width = 256;
            image_size.height = 256;
        }
        
        std::vector<uint8_t> binary_image(image_size.width * image_size.height, 0);
        
        // Fill the binary image with mask points
        for (auto const& point : mask) {
            int x = static_cast<int>(point.x);
            int y = static_cast<int>(point.y);
            
            if (x >= 0 && x < image_size.width && 
                y >= 0 && y < image_size.height) {
                binary_image[y * image_size.width + x] = 1;
            }
        }
        
        std::vector<Point2D<float>> line_points;
        
        if (method == LinePointSelectionMethod::Skeletonize) {
            // Skeletonize the binary image
            auto skeleton = fast_skeletonize(binary_image, image_size.height, image_size.width);
            
            // Order the points of the skeleton
            line_points = order_line(skeleton, image_size, reference_point, subsample);
        } else {
            // Use nearest neighbor ordering starting from reference
            line_points = order_line(binary_image, image_size, reference_point, subsample);
        }
        
        // Remove outliers if requested
        if (should_remove_outliers && line_points.size() > polynomial_order + 2) {
            line_points = remove_outliers(line_points, error_threshold, polynomial_order);
        }
        
        // Add the line to the LineData object
        if (!line_points.empty()) {
            line_data->addLineAtTime(time, line_points);
        }
        
        // Increment the processed mask count
        processed_masks++;
        
        // Report progress after completing each mask, rounded to the nearest percent
        int progress = static_cast<int>(std::round((static_cast<double>(processed_masks) / total_masks) * 100.0));
        progressCallback(progress);
    }
    
    // Ensure final progress is at 100% (in case of rounding errors or empty masks)
    progressCallback(100);
    
    return line_data;
}

std::string MaskToLineOperation::getName() const {
    return "Convert Mask to Line";
}

std::type_index MaskToLineOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskToLineOperation::canApply(DataTypeVariant const& dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<MaskData>>(dataVariant)) {
        return false;
    }

    auto const* ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> MaskToLineOperation::getDefaultParameters() const {
    return std::make_unique<MaskToLineParameters>();
}

DataTypeVariant MaskToLineOperation::execute(DataTypeVariant const& dataVariant, 
                                           TransformParametersBase const* transformParameters) {
    // Call the version with progress reporting but ignore progress
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant MaskToLineOperation::execute(DataTypeVariant const& dataVariant, 
                                           TransformParametersBase const* transformParameters,
                                           ProgressCallback progressCallback) {
    auto const* ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);
    
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "MaskToLineOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};  // Return empty variant
    }
    
    MaskData const* mask_raw_ptr = (*ptr_ptr).get();
    
    MaskToLineParameters const* typed_params = nullptr;
    if (transformParameters) {
        typed_params = dynamic_cast<MaskToLineParameters const*>(transformParameters);
        if (!typed_params) {
            std::cerr << "MaskToLineOperation::execute: Invalid parameter type" << std::endl;
        }
    }
    
    std::shared_ptr<LineData> result_line = mask_to_line(mask_raw_ptr, typed_params, progressCallback);
    
    // Handle potential failure from the calculation function
    if (!result_line) {
        std::cerr << "MaskToLineOperation::execute: 'mask_to_line' failed to produce a result." << std::endl;
        return {};  // Return empty variant
    }
    
    std::cout << "MaskToLineOperation executed successfully." << std::endl;
    return result_line;
} 
