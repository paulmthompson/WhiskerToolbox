/// @file mask_cleaning.cpp
/// @brief Implementation of Mask Cleaning (merge masks per time, keep K largest/smallest CCs).

#include "mask_cleaning.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/utils/connected_component.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <cmath>
#include <iostream>
#include <map>

std::shared_ptr<MaskData> mask_cleaning(
        MaskData const * mask_data,
        MaskCleaningParameters const * params) {
    return mask_cleaning(mask_data, params, [](int) {});
}

std::shared_ptr<MaskData> mask_cleaning(
        MaskData const * mask_data,
        MaskCleaningParameters const * params,
        ProgressCallback progressCallback) {

    if (!mask_data) {
        progressCallback(100);
        return std::make_shared<MaskData>();
    }

    int count = 1;
    bool keep_largest = true;
    if (params) {
        count = params->count;
        if (count < 1) {
            std::cerr << "MaskCleaning: count must be >= 1, using 1" << std::endl;
            count = 1;
        }
        keep_largest = (params->selection == MaskCleaningSelection::Largest);
    }

    auto result_mask_data = std::make_shared<MaskData>();
    result_mask_data->setImageSize(mask_data->getImageSize());
    ImageSize const image_size = mask_data->getImageSize();
    if (image_size.width <= 0 || image_size.height <= 0) {
        progressCallback(100);
        return result_mask_data;
    }

    std::map<TimeFrameIndex, std::vector<Mask2D>> by_time;
    for (auto const & [time, entity_id, mask]: mask_data->flattened_data()) {
        static_cast<void>(entity_id);
        by_time[time].push_back(mask);
    }

    size_t const total_times = by_time.size();
    size_t processed = 0;
    for (auto const & [time, masks]: by_time) {
        Image combined(image_size);
        bool any_foreground = false;
        for (Mask2D const & m: masks) {
            if (m.empty()) {
                continue;
            }
            any_foreground = true;
            Image const bin = mask_to_binary_image(m, image_size);
            for (size_t i = 0; i < combined.data.size(); ++i) {
                if (bin.data[i] != 0) {
                    combined.data[i] = 1;
                }
            }
        }
        if (any_foreground) {
            Image const processed_image = keep_ranked_clusters(combined, count, keep_largest);
            Mask2D const out = binary_image_to_mask(processed_image);
            if (!out.empty()) {
                result_mask_data->addAtTime(time, out, NotifyObservers::No);
            }
        }
        ++processed;
        if (total_times > 0) {
            int const progress = static_cast<int>(std::round(
                    static_cast<double>(processed) / static_cast<double>(total_times) * 100.0));
            progressCallback(progress);
        }
    }
    progressCallback(100);
    return result_mask_data;
}

std::string MaskCleaningOperation::getName() const {
    return "Mask Cleaning";
}

std::type_index MaskCleaningOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskCleaningOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<MaskData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> MaskCleaningOperation::getDefaultParameters() const {
    return std::make_unique<MaskCleaningParameters>();
}

DataTypeVariant MaskCleaningOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant MaskCleaningOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters,
                                               ProgressCallback progressCallback) {

    auto const * mask_data_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);

    if (!mask_data_ptr || !(*mask_data_ptr)) {
        std::cerr << "MaskCleaningOperation::execute: incompatible variant or null mask data." << std::endl;
        return {};
    }

    MaskData const * input_mask_data = (*mask_data_ptr).get();

    MaskCleaningParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;

    if (transformParameters) {
        params = dynamic_cast<MaskCleaningParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "MaskCleaningOperation::execute: invalid parameter type, using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<MaskCleaningParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = static_cast<MaskCleaningParameters const *>(default_params_owner.get());
    }

    if (!params) {
        std::cerr << "MaskCleaningOperation::execute: failed to obtain parameters." << std::endl;
        return {};
    }

    std::shared_ptr<MaskData> result = mask_cleaning(input_mask_data, params, progressCallback);
    if (!result) {
        std::cerr << "MaskCleaningOperation::execute: mask_cleaning returned null." << std::endl;
        return {};
    }

    return result;
}
