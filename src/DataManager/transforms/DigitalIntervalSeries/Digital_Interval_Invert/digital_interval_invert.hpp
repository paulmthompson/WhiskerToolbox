#ifndef WHISKERTOOLBOX_DIGITAL_INTERVAL_INVERT_HPP
#define WHISKERTOOLBOX_DIGITAL_INTERVAL_INVERT_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class DigitalIntervalSeries;

enum class DomainType {
    Bounded,
    Unbounded
};

struct InvertParams : public TransformParametersBase {
    DomainType domainType = DomainType::Unbounded;
    double boundStart = 0.0;    // Used only when domainType is Bounded
    double boundEnd = 100.0;    // Used only when domainType is Bounded
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Inverts intervals in a DigitalIntervalSeries.
 *
 * This function analyzes a digital interval series and creates the inverse/complement
 * of the intervals. The gaps between intervals become the new intervals.
 *
 * For example, with intervals (5,10), (13,20), (23,40), (56,70), (72,91):
 * - Unbounded: Result is (10,13), (20,23), (40,56), (70,72)
 * - Bounded (0,100): Result is (0,5), (10,13), (20,23), (40,56), (70,72), (91,100)
 *
 * @param digital_interval_series The DigitalIntervalSeries to process. Must not be null.
 * @param invertParams Parameters containing domain type and bounds.
 * @return A new DigitalIntervalSeries containing inverted intervals.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<DigitalIntervalSeries> invert_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        InvertParams const & invertParams);

/**
 * @brief Inverts intervals in a DigitalIntervalSeries with progress reporting.
 *
 * This function analyzes a digital interval series and creates the inverse/complement
 * of the intervals. Progress is reported through the provided callback.
 *
 * For example, with intervals (5,10), (13,20), (23,40), (56,70), (72,91):
 * - Unbounded: Result is (10,13), (20,23), (40,56), (70,72)
 * - Bounded (0,100): Result is (0,5), (10,13), (20,23), (40,56), (70,72), (91,100)
 *
 * @param digital_interval_series The DigitalIntervalSeries to process. Must not be null.
 * @param invertParams Parameters containing domain type and bounds.
 * @param progressCallback Function called with progress percentage (0-100) during computation.
 * @return A new DigitalIntervalSeries containing inverted intervals.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<DigitalIntervalSeries> invert_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        InvertParams const & invertParams,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class InvertIntervalOperation final : public TransformOperation {
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
     * @brief Gets default parameters for the invert operation.
     * @return Default InvertParams with Unbounded domain type.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the interval inversion using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the DigitalIntervalSeries object.
     * @param transformParameters Parameters for the inversion (InvertParams).
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the interval inversion with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the DigitalIntervalSeries object.
     * @param transformParameters Parameters for the inversion (InvertParams).
     * @param progressCallback Function called with progress percentage (0-100) during computation.
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_DIGITAL_INTERVAL_INVERT_HPP
