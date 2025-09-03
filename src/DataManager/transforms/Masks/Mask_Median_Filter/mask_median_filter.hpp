#ifndef MASK_MEDIAN_FILTER_HPP
#define MASK_MEDIAN_FILTER_HPP

#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

class MaskData;

struct MaskMedianFilterParameters : public TransformParametersBase {
    /**
     * @brief Size of the square window for median filtering
     * 
     * The median filter uses a square window of this size centered on each pixel.
     * Must be odd and >= 1. Larger windows provide more aggressive smoothing
     * but may remove fine details.
     */
    int window_size = 3;
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Apply median filtering to mask data for noise reduction
 * 
 * This function applies median filtering to remove noise and smooth mask boundaries.
 * Median filtering replaces each pixel with the median value of pixels in its
 * neighborhood, effectively removing small isolated pixels (salt noise) and
 * filling small gaps (pepper noise) in binary images.
 * 
 * @param mask_data The MaskData to filter
 * @param params The median filter parameters containing window size
 * @return A new MaskData with median filtering applied
 */
std::shared_ptr<MaskData> apply_median_filter(
        MaskData const * mask_data,
        MaskMedianFilterParameters const * params = nullptr);

/**
 * @brief Apply median filtering to mask data with progress reporting
 * 
 * @param mask_data The MaskData to filter
 * @param params The median filter parameters containing window size
 * @param progressCallback Progress reporting callback
 * @return A new MaskData with median filtering applied
 */
std::shared_ptr<MaskData> apply_median_filter(
        MaskData const * mask_data,
        MaskMedianFilterParameters const * params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class MaskMedianFilterOperation final : public TransformOperation {
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

#endif // MASK_MEDIAN_FILTER_HPP 