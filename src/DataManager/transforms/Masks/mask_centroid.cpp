#include "mask_centroid.hpp"

#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <cmath>
#include <iostream>

std::shared_ptr<PointData> calculate_mask_centroid(
        MaskData const * mask_data,
        MaskCentroidParameters const * params) {
    return calculate_mask_centroid(mask_data, params, [](int) {});
}

std::shared_ptr<PointData> calculate_mask_centroid(
        MaskData const * mask_data,
        MaskCentroidParameters const * params,
        ProgressCallback progressCallback) {

    static_cast<void>(params);// Parameters not used currently

    auto result_point_data = std::make_shared<PointData>();

    if (!mask_data) {
        progressCallback(100);
        return result_point_data;
    }

    // Copy image size from input mask data
    result_point_data->setImageSize(mask_data->getImageSize());

    // Count total masks to process for progress calculation
    size_t total_masks = 0;
    for (auto const & mask_time_pair: mask_data->getAllAsRange()) {
        if (!mask_time_pair.masks.empty()) {
            total_masks += mask_time_pair.masks.size();
        }
    }

    if (total_masks == 0) {
        progressCallback(100);
        return result_point_data;
    }

    progressCallback(0);

    size_t processed_masks = 0;

    // Process each timestamp
    for (auto const & mask_time_pair: mask_data->getAllAsRange()) {
        auto time = mask_time_pair.time;
        auto const & masks = mask_time_pair.masks;

        // Process each mask at this timestamp
        for (auto const & mask: masks) {
            if (mask.empty()) {
                continue;
            }

            float centroid_x = 0.0f;
            float centroid_y = 0.0f;

            // Calculate centroid
            for (auto const & point: mask) {
                centroid_x += static_cast<float>(point.x);
                centroid_y += static_cast<float>(point.y);
            }

            // Average the coordinates
            centroid_x /= static_cast<float>(mask.size());
            centroid_y /= static_cast<float>(mask.size());

            // Add centroid point to result
            result_point_data->addAtTime(time, {centroid_x, centroid_y}, false);

            processed_masks++;

            // Update progress
            int progress = static_cast<int>(
                    std::round(static_cast<double>(processed_masks) / total_masks * 100.0));
            progressCallback(progress);
        }
    }

    // Notify observers once at the end
    result_point_data->notifyObservers();

    progressCallback(100);

    return result_point_data;
}

///////////////////////////////////////////////////////////////////////////////

std::string MaskCentroidOperation::getName() const {
    return "Calculate Mask Centroid";
}

std::type_index MaskCentroidOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskCentroidOperation::canApply(DataTypeVariant const & dataVariant) const {
    // 1. Check if the variant holds the correct alternative type (shared_ptr<MaskData>)
    if (!std::holds_alternative<std::shared_ptr<MaskData>>(dataVariant)) {
        return false;
    }

    // 2. Check if the shared_ptr it holds is actually non-null.
    //    Use get_if for safe access (returns nullptr if type is wrong, though step 1 checked)
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);

    // Return true only if get_if succeeded AND the contained shared_ptr is not null.
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> MaskCentroidOperation::getDefaultParameters() const {
    return std::make_unique<MaskCentroidParameters>();
}

DataTypeVariant MaskCentroidOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters) {
    // Call the version with progress reporting but ignore progress
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant MaskCentroidOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters,
                                               ProgressCallback progressCallback) {

    // 1. Safely get pointer to the shared_ptr<MaskData> if variant holds it.
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);

    // 2. Validate the pointer from get_if and the contained shared_ptr.
    //    This check ensures we have the right type and it's not a null shared_ptr.
    //    canApply should generally ensure this, but execute should be robust.
    if (!ptr_ptr || !(*ptr_ptr)) {
        // Logically this means canApply would be false. Return empty for graceful failure.
        std::cerr << "MaskCentroidOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty
    }

    // 3. Get the non-owning raw pointer to pass to the calculation function.
    MaskData const * mask_raw_ptr = (*ptr_ptr).get();

    // 4. Cast parameters to the specific type
    auto const * params = dynamic_cast<MaskCentroidParameters const *>(transformParameters);
    if (transformParameters && !params) {
        std::cerr << "MaskCentroidOperation::execute: Invalid parameter type provided." << std::endl;
        return {};
    }

    // 5. Call the core calculation logic.
    std::shared_ptr<PointData> result_point_data = calculate_mask_centroid(mask_raw_ptr, params, progressCallback);

    // 6. Handle potential failure from the calculation function.
    if (!result_point_data) {
        std::cerr << "MaskCentroidOperation::execute: 'calculate_mask_centroid' failed to produce a result." << std::endl;
        return {};// Return empty
    }

    std::cout << "MaskCentroidOperation executed successfully using variant input." << std::endl;
    return result_point_data;
}