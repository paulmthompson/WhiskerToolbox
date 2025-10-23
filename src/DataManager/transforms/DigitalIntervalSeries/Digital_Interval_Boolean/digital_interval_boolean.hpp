#ifndef WHISKERTOOLBOX_DIGITAL_INTERVAL_BOOLEAN_HPP
#define WHISKERTOOLBOX_DIGITAL_INTERVAL_BOOLEAN_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class DigitalIntervalSeries;

struct BooleanParams : public TransformParametersBase {
    enum class BooleanOperation {
        AND,  // Intersection of intervals (both must be true)
        OR,   // Union of intervals (either can be true)
        XOR,  // Exclusive or (exactly one must be true)
        NOT,  // Invert the input series (ignore other_series)
        AND_NOT // Input AND (NOT other) - subtract other from input
    };

    BooleanOperation operation = BooleanOperation::AND;
    std::shared_ptr<DigitalIntervalSeries> other_series;
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Apply boolean logic between two DigitalIntervalSeries.
 * 
 * This function treats intervals as boolean time series where presence in an interval
 * means "true" at that timestamp. It applies the specified boolean operation across
 * the combined time range and reconstructs intervals from the result.
 * 
 * Operations:
 * - AND: Returns intervals where both series have intervals
 * - OR: Returns intervals where either series has an interval
 * - XOR: Returns intervals where exactly one series has an interval
 * - NOT: Returns intervals where the input series does NOT have intervals (ignores other_series)
 * - AND_NOT: Returns intervals where input has intervals but other_series does not
 *
 * @param digital_interval_series The primary DigitalIntervalSeries. Must not be null.
 * @param booleanParams Parameters containing the operation and optional second series.
 * @return A new DigitalIntervalSeries containing the result of the boolean operation.
 *         Returns an empty series if input is null.
 */
std::shared_ptr<DigitalIntervalSeries> apply_boolean_operation(
        DigitalIntervalSeries const * digital_interval_series,
        BooleanParams const & booleanParams);

/**
 * @brief Apply boolean logic between two DigitalIntervalSeries with progress reporting.
 * 
 * This function treats intervals as boolean time series where presence in an interval
 * means "true" at that timestamp. Progress is reported through the provided callback.
 * 
 * @param digital_interval_series The primary DigitalIntervalSeries. Must not be null.
 * @param booleanParams Parameters containing the operation and optional second series.
 * @param progressCallback Function called with progress percentage (0-100) during computation.
 * @return A new DigitalIntervalSeries containing the result of the boolean operation.
 *         Returns an empty series if input is null.
 */
std::shared_ptr<DigitalIntervalSeries> apply_boolean_operation(
        DigitalIntervalSeries const * digital_interval_series,
        BooleanParams const & booleanParams,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class BooleanOperation final : public TransformOperation {
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
     * @brief Gets default parameters for the boolean operation.
     * @return Default BooleanParams with operation = AND.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the boolean operation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the DigitalIntervalSeries object.
     * @param transformParameters Parameters for the operation (BooleanParams).
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the boolean operation with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the DigitalIntervalSeries object.
     * @param transformParameters Parameters for the operation (BooleanParams).
     * @param progressCallback Function called with progress percentage (0-100) during computation.
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_DIGITAL_INTERVAL_BOOLEAN_HPP
