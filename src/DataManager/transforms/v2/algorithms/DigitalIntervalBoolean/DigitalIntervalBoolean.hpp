#ifndef WHISKERTOOLBOX_V2_DIGITAL_INTERVAL_BOOLEAN_HPP
#define WHISKERTOOLBOX_V2_DIGITAL_INTERVAL_BOOLEAN_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>

class DigitalIntervalSeries;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Parameters for digital interval boolean operations
 * 
 * This transform applies boolean logic between two DigitalIntervalSeries.
 * Intervals are treated as boolean time series where presence in an interval
 * means "true" at that timestamp.
 * 
 * Example JSON:
 * ```json
 * {
 *   "operation": "and"
 * }
 * ```
 * 
 * Note: For the NOT operation, only the first input is used. The second input
 * is ignored but must still be provided due to the binary transform signature.
 */
struct DigitalIntervalBooleanParams {
    /**
     * @brief Boolean operation to perform
     * 
     * Supported values:
     * - "and": Intersection of intervals (both must be true)
     * - "or": Union of intervals (either can be true)
     * - "xor": Exclusive or (exactly one must be true)
     * - "not": Invert the first series (second series is ignored)
     * - "and_not": First AND (NOT second) - subtract second from first
     */
    std::optional<std::string> operation;

    // Helper method with default
    std::string getOperation() const {
        return operation.value_or("and");
    }

    // Validation helper
    bool isValidOperation() const {
        auto op = getOperation();
        return op == "and" || op == "or" || op == "xor" || 
               op == "not" || op == "and_not";
    }
};

// ============================================================================
// Transform Implementation (Binary Container Transform)
// ============================================================================

/**
 * @brief Apply boolean logic between two DigitalIntervalSeries
 * 
 * This is a **binary container transform** because:
 * - Requires temporal alignment between two interval series
 * - Must compare overlapping time ranges
 * - Cannot be decomposed into simple element operations
 * 
 * Operations:
 * - AND: Returns intervals where both series have intervals
 * - OR: Returns intervals where either series has an interval
 * - XOR: Returns intervals where exactly one series has an interval
 * - NOT: Returns intervals where the first series does NOT have intervals
 *        (ignores second series, operates within the time range of first)
 * - AND_NOT: Returns intervals where first has intervals but second does not
 * 
 * @param input_series Primary DigitalIntervalSeries
 * @param other_series Secondary DigitalIntervalSeries (ignored for NOT operation)
 * @param params Parameters specifying the boolean operation
 * @param ctx Compute context for progress reporting and cancellation
 * @return New DigitalIntervalSeries containing the result of the boolean operation
 */
std::shared_ptr<DigitalIntervalSeries> digitalIntervalBoolean(
        DigitalIntervalSeries const & input_series,
        DigitalIntervalSeries const & other_series,
        DigitalIntervalBooleanParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_DIGITAL_INTERVAL_BOOLEAN_HPP
