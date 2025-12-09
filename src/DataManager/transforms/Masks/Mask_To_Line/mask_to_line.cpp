#include "mask_to_line.hpp"

#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include "CoreGeometry/line_resampling.hpp"
#include "CoreGeometry/order_line.hpp"
#include "Masks/utils/skeletonize.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>// for std::accumulate
#include <vector>

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LineData> mask_to_line(MaskData const * mask_data, MaskToLineParameters const * params) {
    // Call the version with progress reporting but ignore progress
    return mask_to_line(mask_data, params, [](int) {});
}

std::shared_ptr<LineData> mask_to_line(MaskData const * mask_data,
                                       MaskToLineParameters const * params,
                                       ProgressCallback progressCallback) {

    if (!mask_data) {
        std::cerr << "Error: mask_data is null." << std::endl;
        return std::make_shared<LineData>();
    }

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
    size_t const total_masks = mask_data->getTimeCount();

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

    std::vector<uint8_t> binary_image(static_cast<size_t>(image_size.width * image_size.height), 0);

    // Timing variables
    std::vector<long long> skeletonize_times;
    std::vector<long long> order_line_times;
    std::vector<long long> outlier_removal_times;
    std::vector<long long> map_insertion_times;
    std::vector<long long> smoothing_times;

    size_t processed_masks = 0;
    for (auto const & [time, entity_id, mask]: mask_data->flattened_data()) {

        if (mask.empty()) {
            continue;
        }

        Line2D line_points;

        if (method == LinePointSelectionMethod::Skeletonize) {
            // Zero out the binary image
            std::fill(binary_image.begin(), binary_image.end(), 0);

            for (auto const & point: mask) {
                auto x = static_cast<int>(point.x);
                auto y = static_cast<int>(point.y);

                if (x >= 0 && x < image_size.width &&
                    y >= 0 && y < image_size.height) {
                    binary_image[static_cast<size_t>(y * image_size.width + x)] = 1;
                }
            }

            // Time skeletonization
            auto skeletonize_start = std::chrono::high_resolution_clock::now();
            auto skeleton = fast_skeletonize(binary_image, static_cast<size_t>(image_size.height), static_cast<size_t>(image_size.width));
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
            line_points = order_line(mask.points(), reference_point, input_point_subsample_factor);
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
                                         static_cast<double>(skeletonize_times.size());
                std::cout << "Average skeletonization time: " << skeletonize_avg << " μs" << std::endl;
            }

            if (!order_line_times.empty()) {
                double order_avg = std::accumulate(order_line_times.begin(), order_line_times.end(), 0.0) /
                                   static_cast<double>(order_line_times.size());
                std::cout << "Average order_line time: " << order_avg << " μs" << std::endl;
            }

            if (!outlier_removal_times.empty()) {
                double outlier_avg = std::accumulate(outlier_removal_times.begin(), outlier_removal_times.end(), 0.0) /
                                     static_cast<double>(outlier_removal_times.size());
                std::cout << "Average outlier removal time: " << outlier_avg << " μs" << std::endl;
            }

            if (!map_insertion_times.empty()) {
                double insertion_avg = std::accumulate(map_insertion_times.begin(), map_insertion_times.end(), 0.0) /
                                       static_cast<double>(map_insertion_times.size());
                std::cout << "Average map insertion time: " << insertion_avg << " μs" << std::endl;
            }

            if (!smoothing_times.empty()) {
                double smoothing_avg = std::accumulate(smoothing_times.begin(), smoothing_times.end(), 0.0) /
                                       static_cast<double>(smoothing_times.size());
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

        int progress = static_cast<int>(std::round((static_cast<double>(processed_masks) / static_cast<double>(total_masks)) * 100.0));
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
    return "Convert Mask To Line";
}

std::type_index MaskToLineOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskToLineOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<MaskData>(dataVariant);
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
