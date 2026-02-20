#ifndef GATHER_PIPELINE_EXECUTOR_HPP
#define GATHER_PIPELINE_EXECUTOR_HPP

/**
 * @file GatherPipelineExecutor.hpp
 * @brief Gather-and-execute helpers for interval-row tensor columns (Phase 3, Pattern B).
 *
 * Bridges GatherResult (gather) with TransformPipeline (execute) so that the
 * TensorColumnBuilders layer can dispatch interval-row column computation without
 * any per-type branching.
 *
 * ## Usage Pattern
 *
 * ```cpp
 * // Build a ColumnProviderFn for an interval-row tensor column:
 * auto var = dm.getDataVariant(source_key);
 * return [var, intervals, pipe]() -> std::vector<float> {
 *     return WhiskerToolbox::Gather::gatherAndExecutePipeline(*var, intervals, pipe);
 * };
 * ```
 *
 * ## Pre-conditions
 *
 * The caller is responsible for validating the pipeline before calling
 * gatherAndExecutePipeline():
 *   - pipelineProducesFloat() must return true
 *   - pipeline.hasRangeReduction() should be true (interval rows require a
 *     range reduction to collapse each gathered view to a single scalar)
 *
 * @see TensorColumnBuilders for the calling context
 * @see GatherResult for the gather operation
 * @see TransformPipeline::execute() for pipeline execution
 */

#include "DataManager/DataManagerTypes.hpp"

#include <memory>
#include <vector>

class DigitalIntervalSeries;

namespace WhiskerToolbox::Transforms::V2 {
class TransformPipeline;
}// namespace WhiskerToolbox::Transforms::V2

namespace WhiskerToolbox::Gather {

/**
 * @brief Extract a single float from a DataTypeVariant pipeline output.
 *
 * Called after executing a range-reduction pipeline on one gathered interval
 * view.  The pipeline's output should be a single-element AnalogTimeSeries
 * (when the reduction produces a scalar) or a RaggedAnalogTimeSeries (for
 * ragged intermediate results).
 *
 *  - AnalogTimeSeries  → returns values[0], or NAN if empty
 *  - RaggedAnalogTimeSeries → returns first value at the first time point,
 *                             or NAN if empty
 *  - Any other type   → throws std::runtime_error
 *
 * @param output  Pipeline output wrapped in a DataTypeVariant
 * @return float scalar extracted from the output
 *
 * @throws std::runtime_error if the variant does not hold a float-producing type
 */
[[nodiscard]] float extractSingleFloat(DataTypeVariant const & output);

/**
 * @brief Gather source data over intervals and execute a pipeline on each
 *        gathered view, returning one float per interval.
 *
 * For each interval i the function:
 *   1. Creates a GatherResult view/copy of the source data within
 *      [intervals[i].start, intervals[i].end].
 *   2. Executes @p pipeline on that gathered view (T → DataTypeVariant).
 *   3. Calls extractSingleFloat() on the result.
 *
 * If the source data and @p intervals use different TimeFrames, the interval
 * boundaries are converted to the source's coordinate system before gathering
 * (same logic as TensorColumnBuilders::prepareIntervalsForGather).
 *
 * Dispatch is fully type-erased: the function visits the DataTypeVariant and
 * uses GatherResult<T>::create() + pipeline.execute<T>() for each concrete
 * contained type that satisfies TypeTraits::HasDataTraits<T>.
 *
 * @param source    Source data wrapped in a DataTypeVariant
 * @param intervals DigitalIntervalSeries defining the row intervals
 * @param pipeline  TransformPipeline to execute on each gathered view.
 *                  Should end with a range reduction that collapses the
 *                  gathered data to a single scalar.
 * @return std::vector<float> with one element per interval
 *
 * @throws std::runtime_error if the source type does not satisfy HasDataTraits
 *         or if extractSingleFloat() fails on the pipeline output
 */
[[nodiscard]] std::vector<float> gatherAndExecutePipeline(
        DataTypeVariant const & source,
        std::shared_ptr<DigitalIntervalSeries> intervals,
        WhiskerToolbox::Transforms::V2::TransformPipeline const & pipeline);

}// namespace WhiskerToolbox::Gather

#endif// GATHER_PIPELINE_EXECUTOR_HPP
