#include "TensorColumnBuilders.hpp"

#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/core/TypeChainResolver.hpp"
#include "TransformsV2/extension/gatherResult/GatherPipelineExecutor.hpp"
#include "TransformsV2/extension/gatherResult/RowGatherGeometry.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/ContainerTypeIndex.hpp"
#include "DataManager/utils/DataTypeIndexBridge.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cmath>// NAN
#include <functional>
#include <set>
#include <stdexcept>
#include <utility>
#include <variant>

namespace Neuralyzer::TensorBuilders {

namespace {

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
        Neuralyzer::Transforms::V2::TransformPipeline const & pipeline) {
    std::vector<std::string> names;
    names.reserve(pipeline.size());
    for (std::size_t i = 0; i < pipeline.size(); ++i) {
        names.push_back(pipeline.getStep(i).transform_name);
    }
    return names;
}

[[nodiscard]] bool isFloatCompatibleReductionOutput(std::type_index output_type) {
    return output_type == typeid(float) || output_type == typeid(double) ||
           output_type == typeid(int);
}

std::vector<Neuralyzer::Transforms::V2::PipelineValueStore> buildPipelineValueRowStores(
        DataManager & dm,
        std::vector<PipelineValueBindingRecipe> const & bindings,
        std::size_t row_count) {
    using Neuralyzer::Transforms::V2::executePipeline;
    using Neuralyzer::Transforms::V2::PipelineValueStore;

    std::vector<PipelineValueStore> row_stores(row_count);
    if (bindings.empty()) {
        return row_stores;
    }

    std::set<std::string> store_keys;
    for (auto const & binding: bindings) {
        if (binding.store_key.empty()) {
            throw std::runtime_error("pipeline_value_bindings: store_key must not be empty");
        }
        if (!store_keys.insert(binding.store_key).second) {
            throw std::runtime_error(
                    "pipeline_value_bindings: duplicate store_key '" + binding.store_key + "'");
        }

        auto source_variant = dm.getDataVariant(binding.source_key);
        if (!source_variant) {
            throw std::runtime_error(
                    "pipeline_value_bindings: source_key '" + binding.source_key + "' not found");
        }

        DataTypeVariant binding_variant = *source_variant;
        if (!binding.source_pipeline_json.empty()) {
            auto pipeline_result = Neuralyzer::Transforms::V2::Examples::loadPipelineFromJson(
                    binding.source_pipeline_json);
            if (!pipeline_result) {
                auto const error = pipeline_result.error();
                auto const error_message = error ? std::string(error->what()) : std::string("unknown error");
                throw std::runtime_error(
                        "pipeline_value_bindings: failed to load source_pipeline_json for '" +
                        binding.source_key + "': " + error_message);
            }
            binding_variant = executePipeline(binding_variant, *pipeline_result);
        }

        auto const * events_ptr = std::get_if<std::shared_ptr<DigitalEventSeries>>(&binding_variant);
        if (!events_ptr || !*events_ptr) {
            throw std::runtime_error(
                    "pipeline_value_bindings: source '" + binding.source_key +
                    "' must resolve to DigitalEventSeries");
        }
        auto const & events = **events_ptr;
        if (events.size() != row_count) {
            throw std::runtime_error(
                    "pipeline_value_bindings: source '" + binding.source_key +
                    "' row count does not match tensor row count");
        }

        std::size_t row = 0;
        for (auto const & event: events.view()) {
            row_stores[row].set(binding.store_key, event.time());
            ++row;
        }
    }

    return row_stores;
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
 *        (obtained via dmDataTypeToContainerTypeIndex())
 * @param pipeline               The pipeline to validate
 * @return true if the pipeline's final output is a float-compatible scalar
 */
bool pipelineProducesFloat(
        std::type_index source_container_type,
        Neuralyzer::Transforms::V2::TransformPipeline const & pipeline) {
    using Neuralyzer::Transforms::V2::resolveTypeChain;
    using Neuralyzer::TypeTraits::TypeIndexMapper;

    auto const step_names = getStepNames(pipeline);

    if (!step_names.empty()) {
        // Validate the transform chain. TypeChainResolver supports both
        // element-level and container-level steps.
        auto chain = resolveTypeChain(
                source_container_type,
                std::span<std::string const>{step_names});
        if (!chain.all_valid) {
            return false;
        }

        if (auto const & range_reduction = pipeline.getRangeReduction();
            range_reduction.has_value()) {
            // Range reduction follows the element chain —
            // check the reduction's declared scalar output. Input compatibility
            // is intentionally left to execution because some reductions consume
            // API-facing view elements rather than DataTraits element types.
            auto const & red = *range_reduction;
            return isFloatCompatibleReductionOutput(red.output_type);
        }

        // No range reduction: the chain itself must end at float.
        return chain.output_element_type == typeid(float);
    }

    if (auto const & range_reduction = pipeline.getRangeReduction();
        range_reduction.has_value()) {
        // Range-reduction-only (no element steps).
        auto const & red = *range_reduction;
        return isFloatCompatibleReductionOutput(red.output_type);
    }

    // Empty pipeline (identity / passthrough).
    // Only valid when the source's element type is already float.
    auto element_type = TypeIndexMapper::containerToElement(source_container_type);
    return element_type == typeid(float);
}

}// anonymous namespace

// ============================================================================
// buildIntervalPropertyProvider
// ============================================================================

ColumnProviderFn buildIntervalPropertyProvider(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        IntervalProperty property) {
    if (!intervals) {
        throw std::runtime_error(
                "buildIntervalPropertyProvider: intervals must not be null");
    }

    return [ivals = std::move(intervals), property]() -> std::vector<float> {
        std::vector<float> result;
        result.reserve(ivals->size());

        for (auto const & iv: ivals->view()) {
            switch (property) {
                case IntervalProperty::Start:
                    result.push_back(static_cast<float>(iv.interval.start.getValue()));
                    break;
                case IntervalProperty::End:
                    result.push_back(static_cast<float>(iv.interval.end.getValue()));
                    break;
                case IntervalProperty::Duration:
                    result.push_back(static_cast<float>(iv.interval.end.getValue() - iv.interval.start.getValue()));
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
        int64_t offset) {
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
        for (auto const & t: times) {
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
        std::vector<TimeFrameIndex> const & row_times) {
    return std::visit([&](auto const & ptr) -> std::vector<float> {
        using T = std::remove_reference_t<decltype(*ptr)>;

        if constexpr (std::is_same_v<T, AnalogTimeSeries>) {
            std::vector<float> result;
            result.reserve(row_times.size());
            for (auto const & t: row_times) {
                auto val = ptr->getAtTime(t);
                result.push_back(val.value_or(NAN));
            }
            return result;

        } else if constexpr (std::is_same_v<T, RaggedAnalogTimeSeries>) {
            std::vector<float> result;
            result.reserve(row_times.size());
            for (auto const & t: row_times) {
                auto data = ptr->getDataAtTime(t);
                result.push_back(data.empty() ? NAN : data[0]);
            }
            return result;

        } else {
            throw std::runtime_error(
                    "sampleOutputAtRowTimes: pipeline output is not a float time "
                    "series (AnalogTimeSeries or RaggedAnalogTimeSeries)");
        }
    },
                      output);
}

std::vector<TimeFrameIndex> sampleTimesToTimeFrameIndices(
        DigitalEventSeries const & sample_times,
        TimeFrame const & target_time_frame) {
    std::vector<TimeFrameIndex> result;
    result.reserve(sample_times.size());
    for (auto const & event: sample_times.view()) {
        result.push_back(target_time_frame.getIndexAtTime(event.time(), false));
    }
    return result;
}

std::shared_ptr<TimeFrame> getSourceTimeFrameForSampling(
        DataManager & dm,
        std::string const & source_key) {
    auto source_variant = dm.getDataVariant(source_key);
    if (!source_variant) {
        throw std::runtime_error(
                "buildProviderFromRecipe: source_key '" + source_key + "' not found in DataManager");
    }

    return std::visit([](auto const & ptr) -> std::shared_ptr<TimeFrame> {
        if constexpr (requires { ptr->getTimeFrame(); }) {
            return ptr->getTimeFrame();
        } else {
            return nullptr;
        }
    },
                      *source_variant);
}

}// anonymous namespace

ColumnProviderFn buildPipelineColumnProvider(
        DataManager & dm,
        std::string const & source_key,
        std::vector<TimeFrameIndex> const & row_times,
        Neuralyzer::Transforms::V2::TransformPipeline pipeline) {
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

    auto const src_type_index = Neuralyzer::TypeTraits::dmDataTypeToContainerTypeIndex(src_type);

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
                "output for source '" +
                source_key + "'");
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

        DataTypeVariant const output =
                Neuralyzer::Transforms::V2::executePipeline(*var, pipe);

        return sampleOutputAtRowTimes(output, times);
    };
}

// ============================================================================
// buildIntervalPipelineProvider (Pattern B — generic interval-row)
// ============================================================================

ColumnProviderFn buildIntervalPipelineProvider(
        DataManager & dm,
        std::string const & source_key,
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        Neuralyzer::Transforms::V2::TransformPipeline pipeline) {
    return buildIntervalPipelineProvider(
            dm, source_key, std::move(intervals), std::move(pipeline), {});
}

ColumnProviderFn buildIntervalPipelineProvider(
        DataManager & dm,
        std::string const & source_key,
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        Neuralyzer::Transforms::V2::TransformPipeline pipeline,
        std::vector<Neuralyzer::Transforms::V2::PipelineValueStore> row_stores) {
    if (!intervals) {
        throw std::runtime_error(
                "buildIntervalPipelineProvider: intervals must not be null");
    }

    if (!row_stores.empty() && row_stores.size() != intervals->size()) {
        throw std::runtime_error(
                "buildIntervalPipelineProvider: row store count must match interval count");
    }

    // ── Validate source exists ───────────────────────────────────────────
    auto const src_type = dm.getType(source_key);
    if (src_type == DM_DataType::Unknown) {
        throw std::runtime_error(
                "buildIntervalPipelineProvider: source_key '" + source_key +
                "' not found in DataManager");
    }

    auto const src_type_index = Neuralyzer::TypeTraits::dmDataTypeToContainerTypeIndex(src_type);

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
                "output for source '" +
                source_key + "'");
    }

    // ── Return closure that delegates to gatherAndExecutePipeline ────────
    return [&dm, key = source_key,
            ivals = std::move(intervals),
            pipe = std::move(pipeline),
            stores = std::move(row_stores)]() -> std::vector<float> {
        auto var = dm.getDataVariant(key);
        if (!var) {
            throw std::runtime_error(
                    "buildIntervalPipelineProvider: source '" + key +
                    "' no longer available");
        }
        if (stores.empty()) {
            return Neuralyzer::Gather::gatherAndExecutePipeline(*var, ivals, pipe);
        }
        return Neuralyzer::Gather::gatherAndExecutePipeline(*var, ivals, pipe, stores);
    };
}

// ============================================================================
// buildProviderFromRecipe
// ============================================================================

ColumnProviderFn buildProviderFromRecipe(
        DataManager & dm,
        ColumnRecipe const & recipe,
        std::vector<TimeFrameIndex> const & row_times,
        std::shared_ptr<DigitalIntervalSeries const> const & intervals) {
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
        Neuralyzer::Transforms::V2::TransformPipeline pipeline;
        bool const identity_pipeline = Neuralyzer::Gather::isIdentityRowPipelineJson(recipe.pipeline_json);
        if (!identity_pipeline) {
            auto pipeline_result = Neuralyzer::Transforms::V2::Examples::loadPipelineFromJson(recipe.pipeline_json);
            if (!pipeline_result) {
                auto const error = pipeline_result.error();
                auto const error_message = error ? std::string(error->what()) : std::string("unknown error");
                throw std::runtime_error(
                        "buildProviderFromRecipe: failed to load pipeline from JSON: " +
                        error_message);
            }
            pipeline = std::move(pipeline_result.value());
        }

        auto row_geometry = Neuralyzer::Gather::resolveIntervalRowPipelineGeometry(
                intervals, recipe.row_pipeline_json, intervals->size());

        if (auto const * sample_times = std::get_if<std::shared_ptr<DigitalEventSeries const>>(&row_geometry)) {
            if (pipeline.hasRangeReduction()) {
                throw std::runtime_error(
                        "buildProviderFromRecipe: DigitalEventSeries row_pipeline_json output "
                        "selects sample times and cannot be combined with a range reduction");
            }
            auto source_time_frame = getSourceTimeFrameForSampling(dm, recipe.source_key);
            if (!source_time_frame) {
                throw std::runtime_error(
                        "buildProviderFromRecipe: source '" + recipe.source_key +
                        "' has no TimeFrame for sample-time row pipeline output");
            }
            return buildPipelineColumnProvider(
                    dm,
                    recipe.source_key,
                    sampleTimesToTimeFrameIndices(**sample_times, *source_time_frame),
                    std::move(pipeline));
        }

        auto gather_windows = std::get<std::shared_ptr<DigitalIntervalSeries const>>(std::move(row_geometry));

        if (identity_pipeline) {
            throw std::runtime_error(
                    "buildProviderFromRecipe: interval-row gather columns require a pipeline "
                    "with a range reduction");
        }

        auto row_stores = buildPipelineValueRowStores(
                dm, recipe.pipeline_value_bindings, gather_windows->size());

        if (recipe.pipeline_value_bindings.empty()) {
            return buildIntervalPipelineProvider(
                    dm, recipe.source_key, std::move(gather_windows), std::move(pipeline));
        }

        return buildIntervalPipelineProvider(
                dm,
                recipe.source_key,
                std::move(gather_windows),
                std::move(pipeline),
                std::move(row_stores));
    }

    // 4. Timestamp-row columns → Pattern A (generic pipeline + sample)
    if (row_times.empty()) {
        throw std::runtime_error(
                "buildProviderFromRecipe: timestamp-row column requires non-empty row_times");
    }

    // Load pipeline from JSON (empty or explicit identity JSON = passthrough)
    Neuralyzer::Transforms::V2::TransformPipeline pipeline;
    if (!Neuralyzer::Gather::isIdentityRowPipelineJson(recipe.pipeline_json)) {
        auto pipeline_result = Neuralyzer::Transforms::V2::Examples::loadPipelineFromJson(recipe.pipeline_json);
        if (!pipeline_result) {
            auto const error = pipeline_result.error();
            auto const error_message = error ? std::string(error->what()) : std::string("unknown error");
            throw std::runtime_error(
                    "buildProviderFromRecipe: failed to load pipeline from JSON: " +
                    error_message);
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
        std::vector<std::string> const & source_keys) {
    return [&dm, keys = source_keys](
                   LazyColumnTensorStorage & storage,
                   TensorData & tensor) {
        for (std::size_t col = 0; col < keys.size(); ++col) {
            auto const & key = keys[col];
            if (key.empty()) {
                continue;// no source dependency for this column
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

}// namespace Neuralyzer::TensorBuilders
