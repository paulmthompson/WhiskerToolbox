#include "mask_area.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <iostream>
#include <map>

std::shared_ptr<AnalogTimeSeries> area(MaskData const * mask_data) {
    std::map<int, float> areas;

    for (auto const & [time, entries]: mask_data->getAllEntries()) {
        float area = 0.0f;
        for (auto const & mask: entries) {
            area += static_cast<float>(mask.data.size());
        }
        areas[static_cast<int>(time.getValue())] = area;
    }

    return std::make_shared<AnalogTimeSeries>(areas);
}

///////////////////////////////////////////////////////////////////////////////

std::string MaskAreaOperation::getName() const {
    return "Calculate Area";
}

std::type_index MaskAreaOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

std::unique_ptr<TransformParametersBase> MaskAreaOperation::getDefaultParameters() const {
    return std::make_unique<MaskAreaParameters>();
}

bool MaskAreaOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<MaskData>(dataVariant);
}

DataTypeVariant MaskAreaOperation::execute(DataTypeVariant const & dataVariant,
                                             TransformParametersBase const * transformParameters,
                                             ProgressCallback /*progressCallback*/) {
    return execute(dataVariant, transformParameters);
}

DataTypeVariant MaskAreaOperation::execute(DataTypeVariant const & dataVariant, TransformParametersBase const * transformParameters) {

    static_cast<void>(transformParameters);

    // 1. Safely get pointer to the shared_ptr<MaskData> if variant holds it.
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);

    // 2. Validate the pointer from get_if and the contained shared_ptr.
    //    This check ensures we have the right type and it's not a null shared_ptr.
    //    canApply should generally ensure this, but execute should be robust.
    if (!ptr_ptr || !(*ptr_ptr)) {
        // Logically this means canApply would be false. Return empty std::any for graceful failure.
        std::cerr << "MaskAreaOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty
    }

    // 3. Get the non-owning raw pointer to pass to the calculation function.
    MaskData const * mask_raw_ptr = (*ptr_ptr).get();

    // 4. Call the core calculation logic.
    std::shared_ptr<AnalogTimeSeries> result_ts = area(mask_raw_ptr);

    // 5. Handle potential failure from the calculation function.
    if (!result_ts) {
        std::cerr << "MaskAreaOperation::execute: 'calculate_mask_area' failed to produce a result." << std::endl;
        return {};// Return empty
    }

    std::cout << "MaskAreaOperation executed successfully using variant input." << std::endl;
    return result_ts;
}
