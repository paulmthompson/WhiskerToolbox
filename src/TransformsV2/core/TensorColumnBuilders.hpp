#ifndef WHISKERTOOLBOX_V2_TENSOR_COLUMN_BUILDERS_HPP
#define WHISKERTOOLBOX_V2_TENSOR_COLUMN_BUILDERS_HPP

/**
 * @file TensorColumnBuilders.hpp
 * @brief Builder layer that connects DataManager sources, TransformPipeline
 *        execution, and GatherResult gather+reduce patterns into
 *        ColumnProviderFn closures for LazyColumnTensorStorage.
 *
 * The builders live in TransformsV2 (which depends on DataManager and
 * GatherResult) so they can reference all source types. The resulting
 * ColumnProviderFn closures are plain std::function<std::vector<float>()>
 * — they are injected into LazyColumnTensorStorage, which has no
 * TransformsV2 dependency.
 *
 * @see LazyColumnTensorStorage for the storage backend
 * @see TensorData::createFromLazyColumns() for factory usage
 * @see GatherResult for the gather+reduce pattern
 */

#include "DataManager/Tensors/storage/LazyColumnTensorStorage.hpp" // ColumnProviderFn, ColumnSource
#include "DataManager/Tensors/TensorData.hpp"                      // InvalidationWiringFn

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class DataManager;
class DigitalIntervalSeries;

namespace WhiskerToolbox::Transforms::V2 {
class TransformPipeline;
} // namespace WhiskerToolbox::Transforms::V2

namespace WhiskerToolbox::TensorBuilders {

// ============================================================================
// Interval Property Enum
// ============================================================================

/**
 * @brief Property to extract from row intervals.
 *
 * Used by buildIntervalPropertyProvider() to determine which scalar is
 * produced for each interval row.
 */
enum class IntervalProperty : std::uint8_t {
    Start,    ///< Returns the start time of the interval
    End,      ///< Returns the end time of the interval
    Duration  ///< Returns the duration (end − start) of the interval
};

// ============================================================================
// ColumnRecipe — JSON-serializable column specification
// ============================================================================

/**
 * @brief Describes how one tensor column is computed.
 *
 * A ColumnRecipe captures all the information needed to build a
 * ColumnProviderFn closure.  It is JSON-serializable (via reflect-cpp)
 * for save/load of TensorDesigner configurations.
 *
 * The recipe does NOT store the closure itself — closures capture
 * runtime pointers (DataManager*, shared_ptrs) that are not serializable.
 * Use buildProviderFromRecipe() to reconstruct the closure at runtime.
 */
struct ColumnRecipe {
    /// Display name for this column
    std::string column_name;

    /// DataManager key for the source data (empty for interval-property columns)
    std::string source_key;

    /// JSON string describing the TransformPipeline (empty for passthrough)
    std::string pipeline_json;

    /// If this is an interval-property column, which property to extract.
    /// nullopt means this is a data-source column, not an interval-property column.
    std::optional<IntervalProperty> interval_property;
};

// ============================================================================
// Builder Functions
// ============================================================================

/**
 * @brief Build a ColumnProviderFn that extracts a property (start, end,
 *        or duration) from the row intervals.
 *
 * This does NOT consult any DataManager source — the values come entirely
 * from the interval endpoints.
 *
 * @param intervals Row intervals
 * @param property  Which property to extract
 * @return ColumnProviderFn producing one float per interval
 */
ColumnProviderFn buildIntervalPropertyProvider(
    std::shared_ptr<DigitalIntervalSeries> intervals,
    IntervalProperty property);

/**
 * @brief Build a ColumnProviderFn that samples an AnalogTimeSeries at
 *        each row timestamp plus a fixed offset.
 *
 * For each row i the provider reads source.getAtTime(row_times[i] + offset).
 * Returns NaN for any sample that falls outside the source range.
 *
 * This replaces the old AnalogTimestampOffsetsMultiComputer — each offset
 * produces one call to this function, yielding one column.
 *
 * @param dm         DataManager that owns the source data
 * @param source_key DataManager key for an AnalogTimeSeries
 * @param row_times  Ordered vector of TimeFrameIndex values (one per row)
 * @param offset     Time offset to add to each row timestamp (may be negative)
 * @return ColumnProviderFn producing one float per row
 *
 * @throws std::runtime_error if source_key is not an AnalogTimeSeries
 */
ColumnProviderFn buildAnalogSampleAtOffsetProvider(
    DataManager & dm,
    std::string const & source_key,
    std::vector<TimeFrameIndex> const & row_times,
    int64_t offset);

/**
 * @brief Build a generic ColumnProviderFn for timestamp-row tensors from any
 *        source type (Pattern A).
 *
 * Works with any DataManager source type (AnalogTimeSeries, MaskData,
 * LineData, PointData, etc.) as long as the pipeline transforms the source
 * into float output (AnalogTimeSeries or RaggedAnalogTimeSeries).
 *
 * **Empty pipeline (passthrough):** The source must already be a float type
 * (AnalogTimeSeries or RaggedAnalogTimeSeries). Samples the source directly
 * at each row timestamp.
 *
 * **Non-empty pipeline:** Calls executePipeline() on the full source
 * DataTypeVariant, then samples the resulting AnalogTimeSeries or
 * RaggedAnalogTimeSeries at each row timestamp.
 *
 * Terminal range reductions are rejected — they collapse the entire series
 * to a single scalar, which is not meaningful for timestamp-row columns.
 * For interval-row columns that need range reductions, use
 * buildIntervalPipelineProvider() (Pattern B) instead.
 *
 * Validation is performed at build time via pipelineProducesFloat().
 *
 * @param dm         DataManager that owns the source data
 * @param source_key DataManager key for any supported source type
 * @param row_times  Ordered vector of TimeFrameIndex values (one per row)
 * @param pipeline   TransformPipeline (may be empty for passthrough)
 * @return ColumnProviderFn producing one float per row
 *
 * @throws std::runtime_error if the pipeline does not produce float output,
 *         has a terminal range reduction, or the source is unavailable
 */
ColumnProviderFn buildPipelineColumnProvider(
    DataManager & dm,
    std::string const & source_key,
    std::vector<TimeFrameIndex> const & row_times,
    WhiskerToolbox::Transforms::V2::TransformPipeline pipeline);

/**
 * @brief Build a generic ColumnProviderFn for interval-row tensors from any
 *        source type (Pattern B).
 *
 * Works with any DataManager source type that satisfies HasDataTraits
 * (AnalogTimeSeries, MaskData, LineData, PointData, DigitalEventSeries,
 * DigitalIntervalSeries) by delegating to gatherAndExecutePipeline().
 *
 * For each interval row, the function:
 *   1. Gathers a view/copy of the source data within the interval boundaries
 *   2. Executes the pipeline on that gathered view
 *   3. Extracts a single float from the pipeline output via extractSingleFloat()
 *
 * The pipeline **must** contain a range reduction that collapses each gathered
 * view to a single scalar. Pipelines without a range reduction are rejected
 * at build time.
 *
 * Validation is performed at build time via pipelineProducesFloat().
 *
 * @param dm         DataManager that owns the source data
 * @param source_key DataManager key for any supported source type
 * @param intervals  Shared pointer to the DigitalIntervalSeries defining rows
 * @param pipeline   TransformPipeline that must end with a range reduction
 * @return ColumnProviderFn producing one float per interval
 *
 * @throws std::runtime_error if the pipeline does not produce float output,
 *         lacks a range reduction, or the source is unavailable
 *
 * @note RaggedAnalogTimeSeries sources are not supported (no GatherResult
 *       specialisation exists). Passing one will throw at materialization time.
 */
ColumnProviderFn buildIntervalPipelineProvider(
    DataManager & dm,
    std::string const & source_key,
    std::shared_ptr<DigitalIntervalSeries> intervals,
    WhiskerToolbox::Transforms::V2::TransformPipeline pipeline);

/**
 * @brief Build a ColumnProviderFn from a ColumnRecipe.
 *
 * This is the main entry point for building providers from serialized
 * configurations. It dispatches to the appropriate builder based on the
 * recipe's fields.
 *
 * @param dm         DataManager that owns source data
 * @param recipe     Column specification
 * @param row_times  Row timestamps (for timestamp-row tensors; empty for interval-row)
 * @param intervals  Row intervals (for interval-row tensors; nullptr for timestamp-row)
 * @return ColumnProviderFn
 */
ColumnProviderFn buildProviderFromRecipe(
    DataManager & dm,
    ColumnRecipe const & recipe,
    std::vector<TimeFrameIndex> const & row_times,
    std::shared_ptr<DigitalIntervalSeries> intervals);

// ============================================================================
// Invalidation Wiring
// ============================================================================

/**
 * @brief Build an InvalidationWiringFn that registers DataManager observers
 *        to invalidate tensor columns when source data changes.
 *
 * For each (column_index, source_key) pair, the wiring function registers
 * an observer on the source data. When the source notifies, the wiring
 * callback invalidates the corresponding column and notifies the tensor's
 * own observers.
 *
 * @param dm          DataManager that owns source data
 * @param source_keys Ordered list of DataManager keys, one per column.
 *                    Empty strings mean "no source dependency" (e.g. interval
 *                    property columns) and will be skipped.
 * @return InvalidationWiringFn suitable for TensorData::createFromLazyColumns()
 */
InvalidationWiringFn buildInvalidationWiringFn(
    DataManager & dm,
    std::vector<std::string> const & source_keys);

} // namespace WhiskerToolbox::TensorBuilders

#endif // WHISKERTOOLBOX_V2_TENSOR_COLUMN_BUILDERS_HPP
