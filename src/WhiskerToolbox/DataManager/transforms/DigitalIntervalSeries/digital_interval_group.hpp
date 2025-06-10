#ifndef WHISKERTOOLBOX_DIGITAL_INTERVAL_GROUP_HPP
#define WHISKERTOOLBOX_DIGITAL_INTERVAL_GROUP_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class DigitalIntervalSeries;

struct GroupParams : public TransformParametersBase {
    double maxSpacing = 1.0;// Maximum spacing between intervals to group them together
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Groups nearby intervals in a DigitalIntervalSeries.
 * 
 * This function analyzes a digital interval series and combines intervals that are
 * within a specified spacing of each other. Intervals are grouped if the gap between
 * them is less than or equal to the maxSpacing parameter.
 * 
 * For example, with intervals (1,2), (4,5), (10,11) and maxSpacing=3:
 * - (1,2) and (4,5) have gap = 4-2-1 = 1 ≤ 3, so they group to (1,5)
 * - (4,5) and (10,11) have gap = 10-5-1 = 4 > 3, so they remain separate
 * - Result: (1,5), (10,11)
 *
 * @param digital_interval_series The DigitalIntervalSeries to process. Must not be null.
 * @param groupParams Parameters containing the maximum spacing for grouping.
 * @return A new DigitalIntervalSeries containing grouped intervals.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<DigitalIntervalSeries> group_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        GroupParams const & groupParams);

/**
 * @brief Groups nearby intervals in a DigitalIntervalSeries with progress reporting.
 * 
 * This function analyzes a digital interval series and combines intervals that are
 * within a specified spacing of each other. Progress is reported through the provided callback.
 * 
 * For example, with intervals (1,2), (4,5), (10,11) and maxSpacing=3:
 * - (1,2) and (4,5) have gap = 4-2-1 = 1 ≤ 3, so they group to (1,5)
 * - (4,5) and (10,11) have gap = 10-5-1 = 4 > 3, so they remain separate
 * - Result: (1,5), (10,11)
 *
 * @param digital_interval_series The DigitalIntervalSeries to process. Must not be null.
 * @param groupParams Parameters containing the maximum spacing for grouping.
 * @param progressCallback Function called with progress percentage (0-100) during computation.
 * @return A new DigitalIntervalSeries containing grouped intervals.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<DigitalIntervalSeries> group_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        GroupParams const & groupParams,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class GroupOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null DigitalIntervalSeries shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets default parameters for the group operation.
     * @return Default GroupParams with maxSpacing = 1.0.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the grouping using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the DigitalIntervalSeries object.
     * @param transformParameters Parameters for the grouping (GroupParams).
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the grouping with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the DigitalIntervalSeries object.
     * @param transformParameters Parameters for the grouping (GroupParams).
     * @param progressCallback Function called with progress percentage (0-100) during computation.
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_DIGITAL_INTERVAL_GROUP_HPP