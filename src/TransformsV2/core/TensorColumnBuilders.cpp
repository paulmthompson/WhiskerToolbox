#include "TensorColumnBuilders.hpp"

#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"
#include "Tensors/TensorData.hpp"

#include "GatherPipelineExecutor.hpp"

#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/core/TypeChainResolver.hpp"

#include <cmath>     // NAN
#include <functional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace WhiskerToolbox::TensorBuilders {

namespace {

// ============================================================================
// DM_DataType → std::type_index Mapping
// ============================================================================

/**
 * @brief Map a DM_DataType enum value to the std::type_index of the
 *        corresponding container class.
 *
 * This bridges the DataManager's runtime type enum to the type_index
 * used by TypeChainResolver and TypeIndexMapper for pipeline validation.
 *
 * Uses TypeIndexMapper::stringToContainer() internally so the builder
 * layer does not need to include concrete data type headers (MaskData,
 * LineData, PointData, etc.).
 *
 * @throws std::runtime_error for types that cannot be used as pipeline sources
 *         (Video, Images, Tensor, Time, Unknown).
 */
std::type_index dmTypeToContainerTypeIndex(DM_DataType type) {
    using Transforms::V2::TypeIndexMapper;

    switch (type) {
        case DM_DataType::Analog:          return TypeIndexMapper::stringToContainer("AnalogTimeSeries");
        case DM_DataType::RaggedAnalog:    return TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries");
        case DM_DataType::Mask:            return TypeIndexMapper::stringToContainer("MaskData");
        case DM_DataType::Line:            return TypeIndexMapper::stringToContainer("LineData");
        case DM_DataType::Points:          return TypeIndexMapper::stringToContainer("PointData");
        case DM_DataType::DigitalEvent:    return TypeIndexMapper::stringToContainer("DigitalEventSeries");
        case DM_DataType::DigitalInterval: return TypeIndexMapper::stringToContainer("DigitalIntervalSeries");
        default:
            throw std::runtime_error(
                "dmTypeToContainerTypeIndex: unsupported DM_DataType for pipeline source");
    }
}

// ============================================================================
// Pipeline Validation Helpers
// ============================================================================

/**
 * @brief Extract the ordered transform step names from a pipeline.
 *
 * Used to feed TypeChainResolver::resolveTypeChain() which accepts
 * a span<string const> of step names.
 */
std::vector<std::string> getStepNames(
        Transforms::V2::TransformPipeline const & pipeline) {
    std::vector<std::string> names;
    names.reserve(pipeline.size());
    for (std::size_t i = 0; i < pipeline.size(); ++i) {
        names.push_back(pipeline.getStep(i).transform_name);
    }
    return names;
}

/**
 * @brief Check whether a pipeline (element steps + optional range reduction)
 *        will produce float output when applied to a source of the given
 *        container type.
 *
 * Validation is entirely data-free — it walks the type chain using
 * TypeChainResolver and checks the range reduction's output_type metadata.
 *
 * Three cases:
 *  1. Element steps present → validate chain via resolveTypeChain().
 *     If a range reduction follows, its output_type must be float-like.
 *     If no reduction, the chain's output_element_type must be float.
 *  2. No element steps but a range reduction → check reduction output_type.
 *  3. Empty pipeline (no steps, no reduction) → source element type must
 *     already be float (AnalogTimeSeries / RaggedAnalogTimeSeries).
 *
 * @param source_container_type  type_index of the source container
 *        (obtained via dmTypeToContainerTypeIndex())
 * @param pipeline               The pipeline to validate
 * @return true if the pipeline's final output is a float-compatible scalar
 */
bool pipelineProducesFloat(
        std::type_index source_container_type,
        Transforms::V2::TransformPipeline const & pipeline) {
    using Transforms::V2::TypeIndexMapper;
    using Transforms::V2::resolveTypeChain;

    auto const step_names = getStepNames(pipeline);

    if (!step_names.empty()) {
        // Validate the element transform chain
        auto chain = resolveTypeChain(
                source_container_type,
                std::span<std::string const>{step_names});
        if (!chain.all_valid) {
            return false;
        }

        if (pipeline.hasRangeReduction()) {
            // Range reduction follows the element chain —
            // check the reduction's declared output type.
            auto const & red = pipeline.getRangeReduction().value();
            return red.output_type == typeid(float)
                || red.output_type == typeid(double)
                || red.output_type == typeid(int);
        }

        // No range reduction: the chain itself must end at float.
        return chain.output_element_type == typeid(float);
    }

    if (pipeline.hasRangeReduction()) {
        // Range-reduction-only (no element steps).
        auto const & red = pipeline.getRangeReduction().value();
        return red.output_type == typeid(float)
            || red.output_type == typeid(double)
            || red.output_type == typeid(int);
    }

    // Empty pipeline (identity / passthrough).
    // Only valid when the source's element type is already float.
    auto element_type = TypeIndexMapper::containerToElement(source_container_type);
    return element_type == typeid(float);
}

} // anonymous namespace

// ============================================================================
// buildIntervalPropertyProvider
// ============================================================================

ColumnProviderFn buildIntervalPropertyProvider(
    std::shared_ptr<DigitalIntervalSeries> intervals,
    IntervalProperty property)
{
    if (!intervals) {
        throw std::runtime_error(
            "buildIntervalPropertyProvider: intervals must not be null");
    }

    return [ivals = std::move(intervals), property]() -> std::vector<float> {
        std::vector<float> result;
        result.reserve(ivals->size());

        for (auto const & iv : ivals->view()) {
            switch (property) {
                case IntervalProperty::Start:
                    result.push_back(static_cast<float>(iv.interval.start));
                    break;
                case IntervalProperty::End:
                    result.push_back(static_cast<float>(iv.interval.end));
                    break;
                case IntervalProperty::Duration:
                    result.push_back(static_cast<float>(iv.interval.end - iv.interval.start));
                    break;
            }
        }
        return result;
    };
}

// ============================================================================
// buildAnalogSampleAtOffsetProvider
// ============================================================================

ColumnProviderFn buildAnalogSampleAtOffsetProvider(
    DataManager & dm,
    std::string const & source_key,
    std::vector<TimeFrameIndex> const & row_times,
    int64_t offset)
{
    // Validate source exists
    auto source = dm.getData<AnalogTimeSeries>(source_key);
    if (!source) {
        throw std::runtime_error(
            "buildAnalogSampleAtOffsetProvider: source_key '" + source_key +
            "' not found or is not AnalogTimeSeries");
    }

    return [&dm, key = source_key, times = row_times, off = offset]() -> std::vector<float> {
        auto src = dm.getData<AnalogTimeSeries>(key);
        if (!src) {
            throw std::runtime_error(
                "buildAnalogSampleAtOffsetProvider: source '" + key +
                "' no longer available");
        }

        std::vector<float> result;
        result.reserve(times.size());
        for (auto const & t : times) {
            auto offset_time = TimeFrameIndex(t.getValue() + off);
            auto val = src->getAtTime(offset_time);
            result.push_back(val.value_or(NAN));
        }
        return result;
    };
}

// ============================================================================
// buildPipelineColumnProvider (Pattern A — generic timestamp-row)
// ============================================================================

namespace {

/**
 * @brief Sample an AnalogTimeSeries or RaggedAnalogTimeSeries output at the
 *        given row timestamps, returning one float per row.
 *
 * Dispatches via std::visit on DataTypeVariant.  Any other variant
 * alternative (MediaData, MaskData, etc.) triggers an exception — callers
 * must ensure the pipeline produces float output before calling this.
 */
std::vector<float> sampleOutputAtRowTimes(
    DataTypeVariant const & output,
    std::vector<TimeFrameIndex> const & row_times)
{
    return std::visit([&](auto const & ptr) -> std::vector<float> {
        using T = std::remove_reference_t<decltype(*ptr)>;

        if constexpr (std::is_same_v<T, AnalogTimeSeries>) {
            std::vector<float> result;
            result.reserve(row_times.size());
            for (auto const & t : row_times) {
                auto val = ptr->getAtTime(t);
                result.push_back(val.value_or(NAN));
            }
            return result;

        } else if constexpr (std::is_same_v<T, RaggedAnalogTimeSeries>) {
            std::vector<float> result;
            result.reserve(row_times.size());
            for (auto const & t : row_times) {
                auto data = ptr->getDataAtTime(t);
                result.push_back(data.empty() ? NAN : data[0]);
            }
            return result;

        } else {
            throw std::runtime_error(
                "sampleOutputAtRowTimes: pipeline output is not a float time "
                "series (AnalogTimeSeries or RaggedAnalogTimeSeries)");
        }
    }, output);
}

} // anonymous namespace

ColumnProviderFn buildPipelineColumnProvider(
    DataManager & dm,
    std::string const & source_key,
    std::vector<TimeFrameIndex> const & row_times,
    Transforms::V2::TransformPipeline pipeline)
{
    if (row_times.empty()) {
        throw std::runtime_error(
            "buildPipelineColumnProvider: row_times must not be empty");
    }

    // ── Validate source exists ───────────────────────────────────────────
    auto const src_type = dm.getType(source_key);
    if (src_type == DM_DataType::Unknown) {
        throw std::runtime_error(
            "buildPipelineColumnProvider: source_key '" + source_key +
            "' not found in DataManager");
    }

    auto const src_type_index = dmTypeToContainerTypeIndex(src_type);

    bool const is_empty_pipeline = pipeline.empty() && !pipeline.hasRangeReduction();

    // ── Reject terminal range reductions for timestamp rows ─────────────
    if (pipeline.hasRangeReduction()) {
        throw std::runtime_error(
            "buildPipelineColumnProvider: terminal range reductions are not "
            "appropriate for timestamp-row columns (they collapse the entire "
            "series to a single scalar). Use buildIntervalPipelineProvider() "
            "for interval-row columns instead.");
    }

    // ── Validate pipeline produces float output ─────────────────────────
    if (!pipelineProducesFloat(src_type_index, pipeline)) {
        throw std::runtime_error(
            "buildPipelineColumnProvider: pipeline does not produce float "
            "output for source '" + source_key + "'");
    }

    // ── Empty pipeline (passthrough): sample source directly ────────────
    if (is_empty_pipeline) {
        return [&dm, key = source_key,
                times = row_times]() -> std::vector<float> {
            auto var = dm.getDataVariant(key);
            if (!var) {
                throw std::runtime_error(
                    "buildPipelineColumnProvider(passthrough): source '" +
                    key + "' no longer available");
            }
            return sampleOutputAtRowTimes(*var, times);
        };
    }

    // ── Non-empty pipeline: execute then sample ─────────────────────────
    return [&dm, key = source_key, times = row_times,
            pipe = std::move(pipeline)]() -> std::vector<float> {
        auto var = dm.getDataVariant(key);
        if (!var) {
            throw std::runtime_error(
                "buildPipelineColumnProvider: source '" + key +
                "' no longer available");
        }

        DataTypeVariant output =
            Transforms::V2::executePipeline(*var, pipe);

        return sampleOutputAtRowTimes(output, times);
    };
}

// ============================================================================
// buildIntervalPipelineProvider (Pattern B — generic interval-row)
// ============================================================================

ColumnProviderFn buildIntervalPipelineProvider(
    DataManager & dm,
    std::string const & source_key,
    std::shared_ptr<DigitalIntervalSeries> intervals,
    Transforms::V2::TransformPipeline pipeline)
{
    if (!intervals) {
        throw std::runtime_error(
            "buildIntervalPipelineProvider: intervals must not be null");
    }

    // ── Validate source exists ───────────────────────────────────────────
    auto const src_type = dm.getType(source_key);
    if (src_type == DM_DataType::Unknown) {
        throw std::runtime_error(
            "buildIntervalPipelineProvider: source_key '" + source_key +
            "' not found in DataManager");
    }

    auto const src_type_index = dmTypeToContainerTypeIndex(src_type);

    // ── Require a range reduction for interval rows ─────────────────────
    if (!pipeline.hasRangeReduction()) {
        throw std::runtime_error(
            "buildIntervalPipelineProvider: pipeline must have a range "
            "reduction set (interval rows require collapsing each gathered "
            "view to a single scalar)");
    }

    // ── Validate pipeline produces float output ─────────────────────────
    if (!pipelineProducesFloat(src_type_index, pipeline)) {
        throw std::runtime_error(
            "buildIntervalPipelineProvider: pipeline does not produce float "
            "output for source '" + source_key + "'");
    }

    // ── Return closure that delegates to gatherAndExecutePipeline ────────
    return [&dm, key = source_key,
            ivals = std::move(intervals),
            pipe = std::move(pipeline)]() -> std::vector<float> {
        auto var = dm.getDataVariant(key);
        if (!var) {
            throw std::runtime_error(
                "buildIntervalPipelineProvider: source '" + key +
                "' no longer available");
        }
        return WhiskerToolbox::Gather::gatherAndExecutePipeline(*var, ivals, pipe);
    };
}

// ============================================================================
// buildProviderFromRecipe
// ============================================================================

ColumnProviderFn buildProviderFromRecipe(
    DataManager & dm,
    ColumnRecipe const & recipe,
    std::vector<TimeFrameIndex> const & row_times,
    std::shared_ptr<DigitalIntervalSeries> intervals)
{
    // 1. Interval-property column (no data source needed)
    if (recipe.interval_property.has_value()) {
        if (!intervals) {
            throw std::runtime_error(
                "buildProviderFromRecipe: interval_property column requires intervals");
        }
        return buildIntervalPropertyProvider(intervals, recipe.interval_property.value());
    }

    // 2. source_key must be set for all non-interval-property columns
    if (recipe.source_key.empty()) {
        throw std::runtime_error(
            "buildProviderFromRecipe: source_key is empty and no interval_property set");
    }

    // 3. Interval-row columns → Pattern B (generic gather + pipeline)
    if (intervals) {
        if (recipe.pipeline_json.empty()) {
            throw std::runtime_error(
                "buildProviderFromRecipe: interval-row columns require a pipeline "
                "with a range reduction (pipeline_json is empty)");
        }

        auto pipeline_result = Transforms::V2::Examples::loadPipelineFromJson(recipe.pipeline_json);
        if (!pipeline_result) {
            throw std::runtime_error(
                "buildProviderFromRecipe: failed to load pipeline from JSON: " +
                std::string(pipeline_result.error()->what()));
        }

        return buildIntervalPipelineProvider(
            dm, recipe.source_key, std::move(intervals), std::move(pipeline_result.value()));
    }

    // 4. Timestamp-row columns → Pattern A (generic pipeline + sample)
    if (row_times.empty()) {
        throw std::runtime_error(
            "buildProviderFromRecipe: timestamp-row column requires non-empty row_times");
    }

    // Load pipeline from JSON (empty JSON = empty pipeline = identity passthrough)
    Transforms::V2::TransformPipeline pipeline;
    if (!recipe.pipeline_json.empty()) {
        auto pipeline_result = Transforms::V2::Examples::loadPipelineFromJson(recipe.pipeline_json);
        if (!pipeline_result) {
            throw std::runtime_error(
                "buildProviderFromRecipe: failed to load pipeline from JSON: " +
                std::string(pipeline_result.error()->what()));
        }
        pipeline = std::move(pipeline_result.value());
    }

    return buildPipelineColumnProvider(
        dm, recipe.source_key, row_times, std::move(pipeline));
}

// ============================================================================
// buildInvalidationWiringFn
// ============================================================================

InvalidationWiringFn buildInvalidationWiringFn(
    DataManager & dm,
    std::vector<std::string> const & source_keys)
{
    return [&dm, keys = source_keys](
               LazyColumnTensorStorage & storage,
               TensorData & tensor) {
        for (std::size_t col = 0; col < keys.size(); ++col) {
            auto const & key = keys[col];
            if (key.empty()) {
                continue; // no source dependency for this column
            }

            // Register a DataManager observer that invalidates this column
            // and notifies the tensor's observers.
            [[maybe_unused]] auto cb_id =
                dm.addCallbackToData(key, [&storage, &tensor, col]() {
                    storage.invalidateColumn(col);
                    tensor.notifyObservers();
                });
        }
    };
}

} // namespace WhiskerToolbox::TensorBuilders
