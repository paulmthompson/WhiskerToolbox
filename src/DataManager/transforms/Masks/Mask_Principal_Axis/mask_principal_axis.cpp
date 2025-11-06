#include "mask_principal_axis.hpp"

#include "CoreGeometry/masks.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "transforms/utils/variant_type_check.hpp"


#include <algorithm>
#include <cmath>
#include <iostream>

// Helper function to calculate 2x2 eigenvalues and eigenvectors
struct EigenResult {
    float eigenvalue1, eigenvalue2;
    float eigenvector1_x, eigenvector1_y;
    float eigenvector2_x, eigenvector2_y;
    bool success = false;
};

EigenResult calculate_2x2_eigen(float cxx, float cxy, float cyy) {
    EigenResult result;

    // For 2x2 symmetric matrix [[cxx, cxy], [cxy, cyy]]
    // Eigenvalues: lambda = (cxx + cyy)/2 ± sqrt(((cxx - cyy)/2)^2 + cxy^2)
    float trace = cxx + cyy;
    float det = cxx * cyy - cxy * cxy;
    float discriminant = trace * trace / 4.0f - det;

    if (discriminant < 0) {
        std::cerr << "calculate_2x2_eigen: Negative discriminant, using absolute value" << std::endl;
        discriminant = std::abs(discriminant);
    }

    float sqrt_discriminant = std::sqrt(discriminant);
    result.eigenvalue1 = trace / 2.0f + sqrt_discriminant;// Larger eigenvalue
    result.eigenvalue2 = trace / 2.0f - sqrt_discriminant;// Smaller eigenvalue

    // Calculate eigenvectors
    // For eigenvalue1 (larger):
    if (std::abs(cxy) > 1e-6f) {
        result.eigenvector1_x = cxy;
        result.eigenvector1_y = result.eigenvalue1 - cxx;
    } else {
        // Matrix is diagonal - eigenvectors are along coordinate axes
        if (std::abs(cxx - result.eigenvalue1) < 1e-6f) {
            // eigenvalue1 corresponds to x-axis
            result.eigenvector1_x = 1.0f;
            result.eigenvector1_y = 0.0f;
        } else {
            // eigenvalue1 corresponds to y-axis
            result.eigenvector1_x = 0.0f;
            result.eigenvector1_y = 1.0f;
        }
    }

    // Normalize eigenvector1
    float norm1 = std::sqrt(result.eigenvector1_x * result.eigenvector1_x +
                            result.eigenvector1_y * result.eigenvector1_y);
    if (norm1 > 1e-6f) {
        result.eigenvector1_x /= norm1;
        result.eigenvector1_y /= norm1;
    }

    // For eigenvalue2 (smaller): perpendicular to eigenvector1
    result.eigenvector2_x = -result.eigenvector1_y;
    result.eigenvector2_y = result.eigenvector1_x;

    result.success = true;
    return result;
}

// Helper function to extend line to bounding box
std::pair<Point2D<float>, Point2D<float>> extend_line_to_bbox(
        Point2D<float> centroid,
        float direction_x, float direction_y,
        Point2D<uint32_t> bbox_min, Point2D<uint32_t> bbox_max) {

    // Convert bounding box to float for calculations
    float min_x = static_cast<float>(bbox_min.x);
    float min_y = static_cast<float>(bbox_min.y);
    float max_x = static_cast<float>(bbox_max.x);
    float max_y = static_cast<float>(bbox_max.y);

    // Calculate intersection with bounding box edges
    std::vector<Point2D<float>> intersections;

    // Check intersection with each edge
    if (std::abs(direction_x) > 1e-6f) {
        // Left edge (x = min_x)
        float t_left = (min_x - centroid.x) / direction_x;
        float y_left = centroid.y + t_left * direction_y;
        if (y_left >= min_y && y_left <= max_y) {
            intersections.push_back({min_x, y_left});
        }

        // Right edge (x = max_x)
        float t_right = (max_x - centroid.x) / direction_x;
        float y_right = centroid.y + t_right * direction_y;
        if (y_right >= min_y && y_right <= max_y) {
            intersections.push_back({max_x, y_right});
        }
    }

    if (std::abs(direction_y) > 1e-6f) {
        // Bottom edge (y = min_y)
        float t_bottom = (min_y - centroid.y) / direction_y;
        float x_bottom = centroid.x + t_bottom * direction_x;
        if (x_bottom >= min_x && x_bottom <= max_x) {
            intersections.push_back({x_bottom, min_y});
        }

        // Top edge (y = max_y)
        float t_top = (max_y - centroid.y) / direction_y;
        float x_top = centroid.x + t_top * direction_x;
        if (x_top >= min_x && x_top <= max_x) {
            intersections.push_back({x_top, max_y});
        }
    }

    // Find the two intersections furthest apart (or use centroid if we don't have 2)
    if (intersections.size() >= 2) {
        // Remove duplicates (within tolerance)
        for (size_t i = 0; i < intersections.size(); ++i) {
            for (size_t j = i + 1; j < intersections.size();) {
                float dx = intersections[i].x - intersections[j].x;
                float dy = intersections[i].y - intersections[j].y;
                if (dx * dx + dy * dy < 1e-3f) {
                    intersections.erase(intersections.begin() + static_cast<long int>(j));
                } else {
                    ++j;
                }
            }
        }

        if (intersections.size() >= 2) {
            return {intersections[0], intersections[1]};
        }
    }

    // Fallback: create a line from centroid extending in both directions
    // This might be the issue - let's make sure we extend properly along the eigenvector direction
    float extension = std::max(max_x - min_x, max_y - min_y) * 0.5f;
    
    // Extend along the eigenvector direction
    Point2D<float> point1{centroid.x - extension * direction_x,
                         centroid.y - extension * direction_y};
    Point2D<float> point2{centroid.x + extension * direction_x,
                         centroid.y + extension * direction_y};

    // Clamp to bounding box to ensure points are within valid range
    point1.x = std::max(min_x, std::min(max_x, point1.x));
    point1.y = std::max(min_y, std::min(max_y, point1.y));
    point2.x = std::max(min_x, std::min(max_x, point2.x));
    point2.y = std::max(min_y, std::min(max_y, point2.y));

    return {point1, point2};
}

std::shared_ptr<LineData> calculate_mask_principal_axis(
        MaskData const * mask_data,
        MaskPrincipalAxisParameters const * params) {
    return calculate_mask_principal_axis(mask_data, params, [](int) {});
}

std::shared_ptr<LineData> calculate_mask_principal_axis(
        MaskData const * mask_data,
        MaskPrincipalAxisParameters const * params,
        ProgressCallback progressCallback) {

    auto result_line_data = std::make_shared<LineData>();

    if (!mask_data) {
        progressCallback(100);
        return result_line_data;
    }

    // Use default parameters if none provided
    MaskPrincipalAxisParameters default_params;
    if (!params) {
        params = &default_params;
    }

    // Copy image size from input mask data
    result_line_data->setImageSize(mask_data->getImageSize());

    // Count total masks to process for progress calculation
    size_t total_masks = 0;
    for (auto const & mask_time_pair: mask_data->getAllAsRange()) {
        if (!mask_time_pair.masks.empty()) {
            total_masks += mask_time_pair.masks.size();
        }
    }

    if (total_masks == 0) {
        progressCallback(100);
        return result_line_data;
    }

    progressCallback(0);

    size_t processed_masks = 0;

    // Process each timestamp
    for (auto const & mask_time_pair: mask_data->getAllAsRange()) {
        auto time = mask_time_pair.time;
        auto const & masks = mask_time_pair.masks;

        // Calculate principal axis for each mask at this timestamp
        for (auto const & mask: masks) {
            if (mask.size() < 2) {
                processed_masks++;
                continue;// Need at least 2 points for meaningful principal axis
            }

            // Calculate centroid
            double sum_x = 0.0, sum_y = 0.0;
            for (auto const & point: mask) {
                sum_x += static_cast<double>(point.x);
                sum_y += static_cast<double>(point.y);
            }
            float centroid_x = static_cast<float>(sum_x / static_cast<double>(mask.size()));
            float centroid_y = static_cast<float>(sum_y / static_cast<double>(mask.size()));

            // Calculate covariance matrix
            double cxx = 0.0, cxy = 0.0, cyy = 0.0;
            for (auto const & point: mask) {
                double dx = static_cast<double>(point.x) - sum_x / static_cast<double>(mask.size());
                double dy = static_cast<double>(point.y) - sum_y / static_cast<double>(mask.size());
                cxx += dx * dx;
                cxy += dx * dy;
                cyy += dy * dy;
            }

            // Normalize by (n-1) for sample covariance
            double n = static_cast<double>(mask.size());
            if (n > 1) {
                cxx /= (n - 1.0);
                cxy /= (n - 1.0);
                cyy /= (n - 1.0);
            }

            // Calculate eigenvalues and eigenvectors
            EigenResult eigen = calculate_2x2_eigen(static_cast<float>(cxx),
                                                    static_cast<float>(cxy),
                                                    static_cast<float>(cyy));

            if (!eigen.success) {
                processed_masks++;
                continue;
            }

            // Select the desired axis based on parameters
            float direction_x, direction_y;
            if (params->axis_type == PrincipalAxisType::Major) {
                direction_x = eigen.eigenvector1_x;// Major axis (larger eigenvalue)
                direction_y = eigen.eigenvector1_y;
            } else {
                direction_x = eigen.eigenvector2_x;// Minor axis (smaller eigenvalue)
                direction_y = eigen.eigenvector2_y;
            }

            // Get bounding box of the mask
            auto bbox = get_bounding_box(mask);

            // Extend line to bounding box
            auto line_points = extend_line_to_bbox(
                    {centroid_x, centroid_y},
                    direction_x, direction_y,
                    bbox.first, bbox.second);

            // Create line and add to result
            std::vector<Point2D<float>> principal_axis_line = {line_points.first, line_points.second};
            result_line_data->addAtTime(time, principal_axis_line, false);

            processed_masks++;

            // Update progress
            int progress = static_cast<int>(
                    std::round(static_cast<double>(processed_masks) / static_cast<double>(total_masks) * 100.0));
            progressCallback(progress);
        }
    }

    // Notify observers once at the end
    result_line_data->notifyObservers();

    progressCallback(100);

    return result_line_data;
}

///////////////////////////////////////////////////////////////////////////////

std::string MaskPrincipalAxisOperation::getName() const {
    return "Calculate Mask Principal Axis";
}

std::type_index MaskPrincipalAxisOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskPrincipalAxisOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<MaskData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> MaskPrincipalAxisOperation::getDefaultParameters() const {
    return std::make_unique<MaskPrincipalAxisParameters>();
}

DataTypeVariant MaskPrincipalAxisOperation::execute(DataTypeVariant const & dataVariant,
                                                    TransformParametersBase const * transformParameters) {
    // Call the version with progress reporting but ignore progress
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant MaskPrincipalAxisOperation::execute(DataTypeVariant const & dataVariant,
                                                    TransformParametersBase const * transformParameters,
                                                    ProgressCallback progressCallback) {

    // 1. Safely get pointer to the shared_ptr<MaskData> if variant holds it.
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);

    // 2. Validate the pointer from get_if and the contained shared_ptr.
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "MaskPrincipalAxisOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty
    }

    // 3. Get the non-owning raw pointer to pass to the calculation function.
    MaskData const * mask_raw_ptr = (*ptr_ptr).get();

    // 4. Cast parameters to the specific type
    auto const * params = dynamic_cast<MaskPrincipalAxisParameters const *>(transformParameters);
    if (transformParameters && !params) {
        std::cerr << "MaskPrincipalAxisOperation::execute: Invalid parameter type provided." << std::endl;
        return {};
    }

    // 5. Call the core calculation logic.
    std::shared_ptr<LineData> result_line_data = calculate_mask_principal_axis(mask_raw_ptr, params, progressCallback);

    // 6. Handle potential failure from the calculation function.
    if (!result_line_data) {
        std::cerr << "MaskPrincipalAxisOperation::execute: 'calculate_mask_principal_axis' failed to produce a result." << std::endl;
        return {};// Return empty
    }

    std::cout << "MaskPrincipalAxisOperation executed successfully using variant input." << std::endl;
    return result_line_data;
}