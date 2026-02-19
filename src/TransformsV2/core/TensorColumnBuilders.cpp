#include "TensorColumnBuilders.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Tensors/storage/LazyColumnTensorStorage.hpp"
#include "DataManager/Tensors/TensorData.hpp"

#include "GatherResult/GatherResult.hpp"

#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/core/PipelineValueStore.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/extension/RangeReductionTypes.hpp"
#include "TransformsV2/extension/TransformTypes.hpp"
#include "TransformsV2/extension/ViewAdaptorTypes.hpp"

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <cmath>     // NAN
#include <functional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace WhiskerToolbox::TensorBuilders {

namespace {

/**
 * @brief Ensure params is a valid std::any for the reduction registry.
 *
 * If the pipeline's reduction params are empty (has_value() == false),
 * return NoReductionParams{} wrapped in std::any, which is the default
 * expected by stateless reductions like MeanValue, SumValue, etc.
 */
std::any ensureReductionParams(std::any const & params) {
    if (!params.has_value()) {
        return std::any{Transforms::V2::NoReductionParams{}};
    }
    return params;
}

/**
 * @brief Cast the result of executeErased to float, handling int results too.
 *
 * Some reductions (e.g. EventCount) produce int, not float.  We need to
 * return float from our ColumnProviderFn, so handle common numeric types.
 */
float castReductionResult(std::any const & result) {
    if (auto * f = std::any_cast<float>(&result)) {
        return *f;
    }
    if (auto * d = std::any_cast<double>(&result)) {
        return static_cast<float>(*d);
    }
    if (auto * i = std::any_cast<int>(&result)) {
        return static_cast<float>(*i);
    }
    if (auto * l = std::any_cast<long>(&result)) {
        return static_cast<float>(*l);
    }
    if (auto * ll = std::any_cast<long long>(&result)) {
        return static_cast<float>(*ll);
    }
    if (auto * u = std::any_cast<unsigned>(&result)) {
        return static_cast<float>(*u);
    }
    if (auto * sz = std::any_cast<std::size_t>(&result)) {
        return static_cast<float>(*sz);
    }
    throw std::runtime_error("castReductionResult: unsupported reduction output type");
}

/**
 * @brief Prepare intervals for gathering, with cross-TimeFrame conversion if needed.
 *
 * If the source data and interval series have different TimeFrames, the interval
 * boundaries are converted from the interval's coordinate system to the source's
 * coordinate system so that GatherResult queries the correct data range.
 *
 * When no conversion is needed (same TimeFrame, or either is null), the original
 * intervals shared_ptr is returned to avoid unnecessary copies.
 *
 * @param intervals The original interval series (in its own TimeFrame)
 * @param source_tf The source data's TimeFrame
 * @return Shared pointer to intervals with boundaries in the source's TimeFrame
 */
std::shared_ptr<DigitalIntervalSeries> prepareIntervalsForGather(
        std::shared_ptr<DigitalIntervalSeries> const & intervals,
        std::shared_ptr<TimeFrame> const & source_tf) {

    auto interval_tf = intervals->getTimeFrame();

    // No conversion needed if same TimeFrame or either is null
    if (!source_tf || !interval_tf || source_tf.get() == interval_tf.get()) {
        return intervals;
    }

    // Convert interval boundaries from interval's TimeFrame to source's TimeFrame
    auto const & interval_data = intervals->view();
    std::vector<Interval> converted;
    converted.reserve(interval_data.size());

    for (auto const & iv : interval_data) {
        auto [new_start, new_end] = convertTimeFrameRange(
                TimeFrameIndex(iv.interval.start),
                TimeFrameIndex(iv.interval.end),
                *interval_tf, *source_tf);
        converted.push_back(Interval{new_start.getValue(), new_end.getValue()});
    }

    auto result = std::make_shared<DigitalIntervalSeries>(converted);
    result->setTimeFrame(source_tf);
    return result;
}

// ============================================================================
// Multi-Step Pipeline Helpers
// ============================================================================

using ElementVariant = Transforms::V2::ElementVariant;

/**
 * @brief Validate that a pipeline's element steps produce float output.
 *
 * The first step must accept float input (since AnalogTimeSeries values
 * are extracted as floats), and the last step must produce float output
 * (since tensor columns store floats).
 *
 * @throws std::runtime_error if the chain is invalid
 */
void validateStepChainForFloats(Transforms::V2::TransformPipeline const & pipeline) {
    auto & registry = Transforms::V2::ElementRegistry::instance();

    // Validate first step accepts float input
    auto const * first_meta = registry.getMetadata(
        pipeline.getStep(0).transform_name);
    if (!first_meta) {
        throw std::runtime_error(
            "validateStepChainForFloats: transform '" +
            pipeline.getStep(0).transform_name + "' not found in ElementRegistry");
    }
    if (first_meta->input_type != std::type_index(typeid(float))) {
        throw std::runtime_error(
            "validateStepChainForFloats: first step '" +
            pipeline.getStep(0).transform_name +
            "' expects non-float input, but AnalogTimeSeries values are float");
    }

    // Validate last step produces float output
    auto const * last_meta = registry.getMetadata(
        pipeline.getStep(pipeline.size() - 1).transform_name);
    if (!last_meta) {
        throw std::runtime_error(
            "validateStepChainForFloats: transform '" +
            pipeline.getStep(pipeline.size() - 1).transform_name +
            "' not found in ElementRegistry");
    }
    if (last_meta->output_type != std::type_index(typeid(float))) {
        throw std::runtime_error(
            "validateStepChainForFloats: last step '" +
            pipeline.getStep(pipeline.size() - 1).transform_name +
            "' does not produce float output");
    }
}

/**
 * @brief Build a vector of type-erased element transform functions from pipeline steps.
 *
 * Each function in the returned vector wraps one pipeline step and can be
 * applied to an ElementVariant. The chain captures step params by reference
 * (via the pipeline's steps), so parameter binding modifications (from
 * pre-reductions) are visible to the chain.
 *
 * @param pipeline Pipeline whose steps define the transform chain
 * @return Vector of type-erased functions: ElementVariant → ElementVariant
 */
std::vector<std::function<ElementVariant(ElementVariant)>>
buildElementChain(Transforms::V2::TransformPipeline const & pipeline) {
    auto & registry = Transforms::V2::ElementRegistry::instance();

    std::vector<std::function<ElementVariant(ElementVariant)>> chain;
    chain.reserve(pipeline.size());

    for (std::size_t i = 0; i < pipeline.size(); ++i) {
        auto const & step = pipeline.getStep(i);
        auto const * meta = registry.getMetadata(step.transform_name);
        if (!meta) {
            throw std::runtime_error(
                "buildElementChain: transform '" + step.transform_name +
                "' not found in ElementRegistry");
        }

        // Capture &step (reference to pipeline's step) so that binding
        // modifications to step.params are visible to the chain.
        chain.push_back(
            [&registry, name = step.transform_name, &step, meta](
                ElementVariant input) -> ElementVariant {
                return registry.executeWithDynamicParams(
                    name, input, step.params,
                    meta->input_type, meta->output_type, meta->params_type);
            });
    }

    return chain;
}

/**
 * @brief Apply an element transform chain to a float value.
 *
 * Wraps the float in an ElementVariant, applies all chain functions,
 * and extracts the float result.
 *
 * @param chain Vector of element transform functions
 * @param value Input float value
 * @return Transformed float value
 */
float applyChainToFloat(
    std::vector<std::function<ElementVariant(ElementVariant)>> const & chain,
    float value)
{
    ElementVariant current{value};
    for (auto const & fn : chain) {
        current = fn(std::move(current));
    }
    return std::get<float>(std::move(current));
}

/**
 * @brief Execute pre-reductions on a span of elements and populate a PipelineValueStore.
 *
 * Runs each pre-reduction step from the pipeline on the given input span,
 * storing results in the provided value store. The store can then be used
 * to apply bindings to subsequent transform step parameters.
 *
 * @tparam ElementType The element type of the input data (e.g., TimeValuePoint)
 * @param pipeline Pipeline with pre-reductions
 * @param input_span Input data span to reduce
 * @param store Output value store to populate
 */
template<typename ElementType>
void executePreReductionsOnSpan(
    Transforms::V2::TransformPipeline const & pipeline,
    std::span<ElementType const> input_span,
    Transforms::V2::PipelineValueStore & store)
{
    auto & registry = Transforms::V2::RangeReductionRegistry::instance();

    for (auto const & pre_red : pipeline.getPreReductions()) {
        std::any input_any{input_span};
        std::any params_to_use = pre_red.params.has_value()
            ? pre_red.params
            : std::any{Transforms::V2::NoReductionParams{}};

        std::any result_any = registry.executeErased(
            pre_red.reduction_name, pre_red.input_type,
            input_any, params_to_use);

        // Store result in value store, converting to the appropriate type
        if (pre_red.output_type == std::type_index(typeid(float))) {
            store.set(pre_red.output_key, std::any_cast<float>(result_any));
        } else if (pre_red.output_type == std::type_index(typeid(double))) {
            store.set(pre_red.output_key,
                      static_cast<float>(std::any_cast<double>(result_any)));
        } else if (pre_red.output_type == std::type_index(typeid(int))) {
            store.set(pre_red.output_key,
                      static_cast<int64_t>(std::any_cast<int>(result_any)));
        } else if (pre_red.output_type == std::type_index(typeid(int64_t))) {
            store.set(pre_red.output_key, std::any_cast<int64_t>(result_any));
        }
    }
}

/**
 * @brief Apply pre-reduction bindings to all steps in a pipeline.
 *
 * After pre-reductions populate a PipelineValueStore, this function
 * applies the configured bindings to each step's parameters.
 *
 * @param pipeline Pipeline whose steps have parameter bindings
 * @param store Value store populated by pre-reductions
 */
void applyBindingsToAllSteps(
    Transforms::V2::TransformPipeline const & pipeline,
    Transforms::V2::PipelineValueStore const & store)
{
    for (std::size_t s = 0; s < pipeline.size(); ++s) {
        auto const & step = pipeline.getStep(s);
        if (step.hasBindings()) {
            step.applyBindings(store);
        }
    }
}

} // anonymous namespace

// ============================================================================
// buildDirectColumnProvider
// ============================================================================

ColumnProviderFn buildDirectColumnProvider(
    DataManager & dm,
    std::string const & source_key,
    std::vector<TimeFrameIndex> const & row_times)
{
    // Validate source exists and is AnalogTimeSeries
    auto source = dm.getData<AnalogTimeSeries>(source_key);
    if (!source) {
        throw std::runtime_error(
            "buildDirectColumnProvider: source_key '" + source_key +
            "' not found or is not AnalogTimeSeries");
    }

    // Capture source by shared_ptr (keeps it alive) and row_times by value
    return [&dm, key = source_key, times = row_times]() -> std::vector<float> {
        auto src = dm.getData<AnalogTimeSeries>(key);
        if (!src) {
            throw std::runtime_error(
                "buildDirectColumnProvider: source '" + key +
                "' no longer available");
        }

        std::vector<float> result;
        result.reserve(times.size());
        for (auto const & t : times) {
            auto val = src->getAtTime(t);
            result.push_back(val.value_or(NAN));
        }
        return result;
    };
}

// ============================================================================
// buildTimeSeriesColumnProvider
// ============================================================================

ColumnProviderFn buildTimeSeriesColumnProvider(
    DataManager & dm,
    std::string const & source_key,
    std::vector<TimeFrameIndex> const & row_times,
    Transforms::V2::TransformPipeline pipeline)
{
    auto source = dm.getData<AnalogTimeSeries>(source_key);
    if (!source) {
        throw std::runtime_error(
            "buildTimeSeriesColumnProvider: source_key '" + source_key +
            "' not found or is not AnalogTimeSeries");
    }

    if (pipeline.empty() && !pipeline.hasRangeReduction()) {
        // Degenerate case: no transforms at all → fall back to direct
        return buildDirectColumnProvider(dm, source_key, row_times);
    }

    bool const has_steps = !pipeline.empty();
    bool const has_reduction = pipeline.hasRangeReduction();

    // ────────────────────────────────────────────────────────────────
    // Case 1: Element steps only, no range reduction
    // Apply the transform chain to each sample value.
    // ────────────────────────────────────────────────────────────────
    if (has_steps && !has_reduction) {
        validateStepChainForFloats(pipeline);

        // We must capture the pipeline by value so that the chain's &step
        // references remain valid (they point into the captured pipeline).
        return [&dm, key = source_key, times = row_times,
                pipe = std::move(pipeline)]() -> std::vector<float> {
            auto src = dm.getData<AnalogTimeSeries>(key);
            if (!src) {
                throw std::runtime_error(
                    "buildTimeSeriesColumnProvider: source '" + key +
                    "' no longer available");
            }

            auto chain = buildElementChain(pipe);

            std::vector<float> result;
            result.reserve(times.size());
            for (auto const & t : times) {
                auto val = src->getAtTime(t);
                if (!val.has_value()) {
                    result.push_back(NAN);
                    continue;
                }
                result.push_back(applyChainToFloat(chain, val.value()));
            }
            return result;
        };
    }

    // ────────────────────────────────────────────────────────────────
    // Case 2: Element steps + range reduction
    // Transform each sample value, then apply reduction on the single
    // transformed element. (Degenerate but supported for completeness.)
    // ────────────────────────────────────────────────────────────────
    if (has_steps && has_reduction) {
        validateStepChainForFloats(pipeline);

        auto const & reduction = pipeline.getRangeReduction().value();
        auto reduction_name = reduction.reduction_name;
        auto reduction_params = ensureReductionParams(reduction.params);

        return [&dm, key = source_key, times = row_times,
                pipe = std::move(pipeline),
                red_name = std::move(reduction_name),
                red_params = std::move(reduction_params)]() -> std::vector<float> {
            using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;
            auto src = dm.getData<AnalogTimeSeries>(key);
            if (!src) {
                throw std::runtime_error(
                    "buildTimeSeriesColumnProvider: source '" + key +
                    "' no longer available");
            }

            auto chain = buildElementChain(pipe);
            auto & registry = Transforms::V2::RangeReductionRegistry::instance();

            std::vector<float> result;
            result.reserve(times.size());
            for (auto const & t : times) {
                auto val = src->getAtTime(t);
                if (!val.has_value()) {
                    result.push_back(NAN);
                    continue;
                }
                float transformed = applyChainToFloat(chain, val.value());
                TimeValuePoint tvp{t, transformed};
                std::span<TimeValuePoint const> span{&tvp, 1};
                std::any input_any{span};
                std::any res = registry.executeErased(
                    red_name, typeid(TimeValuePoint), input_any, red_params);
                result.push_back(castReductionResult(res));
            }
            return result;
        };
    }

    // ────────────────────────────────────────────────────────────────
    // Case 3: Range reduction only, no element steps (existing behavior)
    // ────────────────────────────────────────────────────────────────
    if (has_reduction) {
        auto const & reduction = pipeline.getRangeReduction().value();
        auto reduction_name = reduction.reduction_name;
        auto reduction_params = ensureReductionParams(reduction.params);

        return [&dm, key = source_key, times = row_times,
                red_name = std::move(reduction_name),
                red_params = std::move(reduction_params)]() -> std::vector<float> {
            using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;
            auto src = dm.getData<AnalogTimeSeries>(key);
            if (!src) {
                throw std::runtime_error(
                    "buildTimeSeriesColumnProvider: source '" + key +
                    "' no longer available");
            }

            auto & registry = Transforms::V2::RangeReductionRegistry::instance();

            std::vector<float> result;
            result.reserve(times.size());
            for (auto const & t : times) {
                auto val = src->getAtTime(t);
                if (!val.has_value()) {
                    result.push_back(NAN);
                    continue;
                }
                TimeValuePoint tvp{t, val.value()};
                std::span<TimeValuePoint const> span{&tvp, 1};
                std::any input_any{span};
                std::any res = registry.executeErased(
                    red_name, typeid(TimeValuePoint), input_any, red_params);
                result.push_back(castReductionResult(res));
            }
            return result;
        };
    }

    // Should not reach here — fall back to direct passthrough as safety net
    return buildDirectColumnProvider(dm, source_key, row_times);
}

// ============================================================================
// buildIntervalReductionProvider — type-specific helpers
// ============================================================================

namespace {

/**
 * @brief Helper: gather AnalogTimeSeries over intervals and reduce.
 *
 * When the pipeline has element transform steps, the flow per interval is:
 *   1. Gather raw TimeValuePoints
 *   2. Execute pre-reductions (MeanValue, StdValue, etc.) on raw data
 *   3. Apply parameter bindings (inject pre-reduction results into step params)
 *   4. Apply element transform chain (float → float) to each gathered value
 *   5. Reconstruct TimeValuePoints with transformed values (preserving times)
 *   6. Apply final range reduction on the transformed TimeValuePoints
 *
 * When the pipeline has no element steps (only a range reduction), the simpler
 * gather → reduce path using GatherResult::reduce() is used.
 *
 * Uses RangeReductionRegistry::executeErased() directly rather than
 * bindReducer<TimeValuePoint, float>() to avoid a template-instantiation
 * issue: TimeValuePoint is not in ElementVariant, so the non-empty-pipeline
 * branch of bindReducer cannot be compiled.  Our pipelines here only carry
 * a range-reduction (no element-transform steps), making direct registry
 * invocation both correct and simpler.
 */
ColumnProviderFn buildIntervalReductionForAnalog(
    DataManager & dm,
    std::string const & source_key,
    std::shared_ptr<DigitalIntervalSeries> intervals,
    Transforms::V2::TransformPipeline pipeline)
{
    using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;

    auto const & reduction = pipeline.getRangeReduction().value();
    auto reduction_name = reduction.reduction_name;
    auto reduction_params = ensureReductionParams(reduction.params);

    bool const has_steps = !pipeline.empty();

    // ────────────────────────────────────────────────────────────────
    // Multi-step path: element transforms (+ optional pre-reductions)
    // ────────────────────────────────────────────────────────────────
    if (has_steps) {
        validateStepChainForFloats(pipeline);

        return [&dm, key = source_key,
                ivals = std::move(intervals),
                pipe = std::move(pipeline),
                red_name = std::move(reduction_name),
                red_params = std::move(reduction_params)]() -> std::vector<float> {
            auto src = dm.getData<AnalogTimeSeries>(key);
            if (!src) {
                throw std::runtime_error(
                    "buildIntervalReductionForAnalog(MultiStep): source '" +
                    key + "' no longer available");
            }

            auto gather_ivals = prepareIntervalsForGather(ivals, src->getTimeFrame());
            auto gather = GatherResult<AnalogTimeSeries>::create(src, gather_ivals);

            auto & reduction_registry =
                Transforms::V2::RangeReductionRegistry::instance();

            std::vector<float> result;
            result.reserve(gather.size());

            for (std::size_t i = 0; i < gather.size(); ++i) {
                // Materialize the gathered view for this interval
                auto view = gather[i]->view();
                std::vector<TimeValuePoint> elements(view.begin(), view.end());
                std::span<TimeValuePoint const> element_span{elements};

                // 1. Execute pre-reductions on RAW gathered data
                Transforms::V2::PipelineValueStore store;
                if (pipe.hasPreReductions()) {
                    executePreReductionsOnSpan(pipe, element_span, store);
                }

                // 2. Apply bindings to step params (e.g., inject mean/std)
                applyBindingsToAllSteps(pipe, store);

                // 3. Build element chain and transform each value
                //    The chain captures &step references into the pipeline,
                //    so binding modifications from step 2 are visible.
                auto chain = buildElementChain(pipe);
                std::vector<TimeValuePoint> transformed;
                transformed.reserve(elements.size());
                for (auto const & tvp : elements) {
                    float new_val = applyChainToFloat(chain, tvp.value());
                    transformed.push_back({tvp.time(), new_val});
                }

                // 4. Apply final range reduction on transformed data
                std::span<TimeValuePoint const> transformed_span{transformed};
                std::any input_any{transformed_span};
                std::any res = reduction_registry.executeErased(
                    red_name, typeid(TimeValuePoint), input_any, red_params);
                result.push_back(castReductionResult(res));
            }

            return result;
        };
    }

    // ────────────────────────────────────────────────────────────────
    // Simple path: range reduction only, no element steps
    // ────────────────────────────────────────────────────────────────

    // Build a ReducerFactoryV2 that creates a ReducerFn on the fly using
    // the erased registry.
    Transforms::V2::ReducerFactoryV2<TimeValuePoint, float> factory =
        [red_name = std::move(reduction_name),
         red_params = std::move(reduction_params)](
            Transforms::V2::PipelineValueStore const &) -> Transforms::V2::ReducerFn<TimeValuePoint, float> {
            return [red_name, red_params](std::span<TimeValuePoint const> input) -> float {
                auto & registry = Transforms::V2::RangeReductionRegistry::instance();
                std::any input_any{input};
                std::any result = registry.executeErased(
                    red_name, typeid(TimeValuePoint), input_any, red_params);
                return castReductionResult(result);
            };
        };

    return [&dm, key = source_key,
            ivals = std::move(intervals),
            fac = std::move(factory)]() -> std::vector<float> {
        auto src = dm.getData<AnalogTimeSeries>(key);
        if (!src) {
            throw std::runtime_error(
                "buildIntervalReductionProvider(Analog): source '" + key +
                "' no longer available");
        }

        auto gather_ivals = prepareIntervalsForGather(ivals, src->getTimeFrame());
        auto gather = GatherResult<AnalogTimeSeries>::create(src, gather_ivals);
        return gather.template reduce<float>(fac);
    };
}

/**
 * @brief Helper: gather DigitalEventSeries over intervals and reduce.
 */
ColumnProviderFn buildIntervalReductionForEvents(
    DataManager & dm,
    std::string const & source_key,
    std::shared_ptr<DigitalIntervalSeries> intervals,
    Transforms::V2::TransformPipeline pipeline)
{
    using EventWithId = WhiskerToolbox::Gather::element_type_of_t<DigitalEventSeries>;

    auto const & reduction = pipeline.getRangeReduction().value();
    auto reduction_name = reduction.reduction_name;
    auto reduction_params = ensureReductionParams(reduction.params);

    Transforms::V2::ReducerFactoryV2<EventWithId, float> factory =
        [red_name = std::move(reduction_name),
         red_params = std::move(reduction_params)](
            Transforms::V2::PipelineValueStore const &) -> Transforms::V2::ReducerFn<EventWithId, float> {
            return [red_name, red_params](std::span<EventWithId const> input) -> float {
                auto & registry = Transforms::V2::RangeReductionRegistry::instance();
                std::any input_any{input};
                std::any result = registry.executeErased(
                    red_name, typeid(EventWithId), input_any, red_params);
                return castReductionResult(result);
            };
        };

    return [&dm, key = source_key,
            ivals = std::move(intervals),
            fac = std::move(factory)]() -> std::vector<float> {
        auto src = dm.getData<DigitalEventSeries>(key);
        if (!src) {
            throw std::runtime_error(
                "buildIntervalReductionProvider(Events): source '" + key +
                "' no longer available");
        }

        auto gather_ivals = prepareIntervalsForGather(ivals, src->getTimeFrame());
        auto gather = GatherResult<DigitalEventSeries>::create(src, gather_ivals);
        return gather.template reduce<float>(fac);
    };
}

/**
 * @brief Helper: gather DigitalIntervalSeries over intervals and reduce.
 */
ColumnProviderFn buildIntervalReductionForIntervals(
    DataManager & dm,
    std::string const & source_key,
    std::shared_ptr<DigitalIntervalSeries> intervals,
    Transforms::V2::TransformPipeline pipeline)
{
    using IntervalWithId = WhiskerToolbox::Gather::element_type_of_t<DigitalIntervalSeries>;

    auto const & reduction = pipeline.getRangeReduction().value();
    auto reduction_name = reduction.reduction_name;
    auto reduction_params = ensureReductionParams(reduction.params);

    Transforms::V2::ReducerFactoryV2<IntervalWithId, float> factory =
        [red_name = std::move(reduction_name),
         red_params = std::move(reduction_params)](
            Transforms::V2::PipelineValueStore const &) -> Transforms::V2::ReducerFn<IntervalWithId, float> {
            return [red_name, red_params](std::span<IntervalWithId const> input) -> float {
                auto & registry = Transforms::V2::RangeReductionRegistry::instance();
                std::any input_any{input};
                std::any result = registry.executeErased(
                    red_name, typeid(IntervalWithId), input_any, red_params);
                return castReductionResult(result);
            };
        };

    return [&dm, key = source_key,
            ivals = std::move(intervals),
            fac = std::move(factory)]() -> std::vector<float> {
        auto src = dm.getData<DigitalIntervalSeries>(key);
        if (!src) {
            throw std::runtime_error(
                "buildIntervalReductionProvider(Intervals): source '" + key +
                "' no longer available");
        }

        auto gather_ivals = prepareIntervalsForGather(ivals, src->getTimeFrame());
        auto gather = GatherResult<DigitalIntervalSeries>::create(src, gather_ivals);
        return gather.template reduce<float>(fac);
    };
}

} // anonymous namespace

// ============================================================================
// buildIntervalReductionProvider — public dispatcher
// ============================================================================

ColumnProviderFn buildIntervalReductionProvider(
    DataManager & dm,
    std::string const & source_key,
    std::shared_ptr<DigitalIntervalSeries> intervals,
    Transforms::V2::TransformPipeline pipeline)
{
    if (!intervals) {
        throw std::runtime_error(
            "buildIntervalReductionProvider: intervals must not be null");
    }
    if (!pipeline.hasRangeReduction()) {
        throw std::runtime_error(
            "buildIntervalReductionProvider: pipeline must have a range reduction set");
    }

    // Dispatch on source data type
    auto const type = dm.getType(source_key);

    switch (type) {
        case DM_DataType::Analog:
            return buildIntervalReductionForAnalog(
                dm, source_key, std::move(intervals), std::move(pipeline));

        case DM_DataType::DigitalEvent:
            return buildIntervalReductionForEvents(
                dm, source_key, std::move(intervals), std::move(pipeline));

        case DM_DataType::DigitalInterval:
            return buildIntervalReductionForIntervals(
                dm, source_key, std::move(intervals), std::move(pipeline));

        default:
            throw std::runtime_error(
                "buildIntervalReductionProvider: unsupported source type for key '" +
                source_key + "' (type=" + std::to_string(static_cast<int>(type)) + ")");
    }
}

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
// buildProviderFromRecipe
// ============================================================================

ColumnProviderFn buildProviderFromRecipe(
    DataManager & dm,
    ColumnRecipe const & recipe,
    std::vector<TimeFrameIndex> const & row_times,
    std::shared_ptr<DigitalIntervalSeries> intervals)
{
    // Case 1: Interval-property column (no data source needed)
    if (recipe.interval_property.has_value()) {
        if (!intervals) {
            throw std::runtime_error(
                "buildProviderFromRecipe: interval_property column requires intervals");
        }
        return buildIntervalPropertyProvider(intervals, recipe.interval_property.value());
    }

    if (recipe.source_key.empty()) {
        throw std::runtime_error(
            "buildProviderFromRecipe: source_key is empty and no interval_property set");
    }

    // Case 2: Interval-row columns (with gather + reduction)
    if (intervals) {
        // Build pipeline from JSON if provided
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

        return buildIntervalReductionProvider(
            dm, recipe.source_key, std::move(intervals), std::move(pipeline_result.value()));
    }

    // Case 3: Timestamp-row columns
    if (row_times.empty()) {
        throw std::runtime_error(
            "buildProviderFromRecipe: timestamp-row column requires non-empty row_times");
    }

    if (recipe.pipeline_json.empty()) {
        // Direct passthrough
        return buildDirectColumnProvider(dm, recipe.source_key, row_times);
    }

    auto pipeline_result = Transforms::V2::Examples::loadPipelineFromJson(recipe.pipeline_json);
    if (!pipeline_result) {
        throw std::runtime_error(
            "buildProviderFromRecipe: failed to load pipeline from JSON: " +
            std::string(pipeline_result.error()->what()));
    }

    return buildTimeSeriesColumnProvider(
        dm, recipe.source_key, row_times, std::move(pipeline_result.value()));
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
