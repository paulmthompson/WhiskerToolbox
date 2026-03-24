#ifndef WHISKERTOOLBOX_MASK_CLEANING_HPP
#define WHISKERTOOLBOX_MASK_CLEANING_HPP

/// @file mask_cleaning.hpp
/// @brief Mask Cleaning transform: per-timestamp merge and ranked connected-component filtering.

#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

class MaskData;

enum class MaskCleaningSelection {
    Largest,
    Smallest
};

struct MaskCleaningParameters : public TransformParametersBase {
    MaskCleaningSelection selection = MaskCleaningSelection::Largest;
    /// Number of connected components to keep per frame (>= 1)
    int count = 1;
};

/**
 * @brief Per timestamp, merge all mask entries into one binary image, then keep the K largest or
 *        smallest foreground connected components (8-connectivity) and write a single output mask.
 */
std::shared_ptr<MaskData> mask_cleaning(
        MaskData const * mask_data,
        MaskCleaningParameters const * params = nullptr);

std::shared_ptr<MaskData> mask_cleaning(
        MaskData const * mask_data,
        MaskCleaningParameters const * params,
        ProgressCallback progressCallback);

class MaskCleaningOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_MASK_CLEANING_HPP
