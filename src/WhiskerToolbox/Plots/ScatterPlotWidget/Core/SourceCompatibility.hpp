#ifndef SCATTER_SOURCE_COMPATIBILITY_HPP
#define SCATTER_SOURCE_COMPATIBILITY_HPP

/**
 * @file SourceCompatibility.hpp
 * @brief Row-type compatibility validation for scatter plot axis sources
 *
 * Before building scatter point data, the widget must verify that the
 * selected X and Y data sources share a compatible row type. This header
 * provides a pure-logic validation function with no Qt or OpenGL dependency.
 *
 * ## Compatibility Matrix
 *
 * | X Source              | Y Source              | Compatible? |
 * |-----------------------|-----------------------|-------------|
 * | AnalogTimeSeries      | AnalogTimeSeries      | Always      |
 * | AnalogTimeSeries      | TensorData (TFI rows) | Yes         |
 * | TensorData (TFI)      | AnalogTimeSeries      | Yes         |
 * | TensorData (TFI)      | TensorData (TFI)      | Yes         |
 * | TensorData (Interval) | TensorData (Interval) | Yes         |
 * | TensorData (Ordinal)  | TensorData (Ordinal)  | Yes (same N)|
 * | Other combinations    | —                     | No          |
 *
 * @see ScatterAxisSource for the source descriptor struct
 * @see RowDescriptor / RowType for TensorData row semantics
 */

#include <string>

class DataManager;
struct ScatterAxisSource;

/**
 * @brief Resolved row type of one scatter axis source
 *
 * This mirrors `RowType` but adds `AnalogTimeSeries` as a first-class
 * category so the compatibility function can reason about mixed pairings.
 */
enum class ScatterSourceRowType {
    AnalogTimeSeries,       ///< Source is an AnalogTimeSeries (always TimeFrameIndex)
    TensorTimeFrameIndex,   ///< Source is a TensorData with RowType::TimeFrameIndex
    TensorInterval,         ///< Source is a TensorData with RowType::Interval
    TensorOrdinal,          ///< Source is a TensorData with RowType::Ordinal
    Unknown                 ///< Key not found or data type not supported
};

/**
 * @brief Result of a source compatibility check
 */
struct SourceCompatibilityResult {
    bool compatible = false;                ///< Whether the two sources can be paired
    ScatterSourceRowType x_row_type = ScatterSourceRowType::Unknown;
    ScatterSourceRowType y_row_type = ScatterSourceRowType::Unknown;
    std::string warning_message;            ///< Human-readable reason when incompatible

    /**
     * @brief true when both sources were found in the DataManager
     */
    [[nodiscard]] bool bothSourcesResolved() const {
        return x_row_type != ScatterSourceRowType::Unknown
            && y_row_type != ScatterSourceRowType::Unknown;
    }
};

/**
 * @brief Resolve the scatter source row type for a single axis source
 *
 * Looks up @p source.data_key in @p dm. If the key is an AnalogTimeSeries,
 * returns `ScatterSourceRowType::AnalogTimeSeries`. If the key is a
 * TensorData, returns the row type from its RowDescriptor. Otherwise returns
 * `ScatterSourceRowType::Unknown`.
 *
 * @param dm      DataManager to resolve the key against
 * @param source  Axis source descriptor
 * @return The resolved row type
 */
[[nodiscard]] ScatterSourceRowType resolveSourceRowType(
    DataManager & dm,
    ScatterAxisSource const & source);

/**
 * @brief Check whether two scatter axis sources are row-type compatible
 *
 * This function resolves the row type of each source and checks the
 * compatibility matrix documented above. For TensorData Ordinal × Ordinal
 * pairings, compatibility additionally requires that the two tensors have the
 * same row count.
 *
 * @param dm        DataManager to resolve keys against
 * @param x_source  X-axis data source descriptor
 * @param y_source  Y-axis data source descriptor
 * @return A SourceCompatibilityResult with compatibility flag and diagnostic
 */
[[nodiscard]] SourceCompatibilityResult checkSourceCompatibility(
    DataManager & dm,
    ScatterAxisSource const & x_source,
    ScatterAxisSource const & y_source);

#endif  // SCATTER_SOURCE_COMPATIBILITY_HPP
