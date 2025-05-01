#ifndef WHISKERTOOLBOX_MASK_AREA_HPP
#define WHISKERTOOLBOX_MASK_AREA_HPP

#include "transforms/data_transforms.hpp"

#include <any>          // std::any
#include <memory>       // std::shared_ptr
#include <string>       // std::string
#include <typeindex>    // std::type_index

class AnalogTimeSeries;
class MaskData;

/**
 * @brief Calculate the area of masks at each timestamp
 *
 * @param mask_data The mask data to calculate areas from
 * @return A new AnalogTimeSeries containing area values at each timestamp
 */
std::shared_ptr<AnalogTimeSeries> area(MaskData const * mask_data);


class MaskAreaOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;


    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null MaskData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Executes the mask area calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @return std::any containing a std::shared_ptr<AnalogTimeSeries> on success,
     * or an empty std::any on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    std::any execute(DataTypeVariant const & dataVariant) override;
};

#endif//WHISKERTOOLBOX_MASK_AREA_HPP
