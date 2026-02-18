#include "TensorColumnBuilders.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Tensors/storage/LazyColumnTensorStorage.hpp"
#include "DataManager/Tensors/TensorData.hpp"

#include "GatherResult/GatherResult.hpp"

#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/extension/RangeReductionTypes.hpp"
#include "TransformsV2/extension/ViewAdaptorTypes.hpp"

#include <cmath>     // NAN
#include <stdexcept>
#include <utility>

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

    // If the pipeline has a range reduction set, we use bindReducer to get
    // a reducer that transforms+reduces. For per-element timestamp columns,
    // however, we usually won't have a range reduction — each row produces
    // exactly one value, not a reduced span.
    //
    // Case 1: pipeline has steps but NO range reduction → apply element
    //         transforms to sample value at each timestamp, return transformed value.
    // Case 2: pipeline HAS range reduction → treat each single-sample as a
    //         one-element span and reduce.

    // For timestamp-row columns with a reduction, apply the reduction to
    // each single-sample span.  We build the reducer via the RangeReductionRegistry
    // directly (since TimeValuePoint is not in ElementVariant and cannot use
    // the generic buildComposedTransformFn path in bindReducer).
    //
    // For pipelines with element-transform steps but no reduction, we fall
    // back to passthrough (element transforms on single-sample timeseries
    // columns are an advanced use case to be added later).

    if (pipeline.hasRangeReduction()) {
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

    // Pipeline has steps but no range reduction → element-level transform only.
    // For now, fall back to passthrough; full element-transform chains for
    // single-sample timestamp columns will be added when the ElementRegistry
    // gains a single-element execution API.

    return [&dm, key = source_key, times = row_times]() -> std::vector<float> {
        auto src = dm.getData<AnalogTimeSeries>(key);
        if (!src) {
            throw std::runtime_error(
                "buildTimeSeriesColumnProvider: source '" + key +
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
// buildIntervalReductionProvider — type-specific helpers
// ============================================================================

namespace {

/**
 * @brief Helper: gather AnalogTimeSeries over intervals and reduce.
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

        auto gather = GatherResult<AnalogTimeSeries>::create(src, ivals);
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

        auto gather = GatherResult<DigitalEventSeries>::create(src, ivals);
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

        auto gather = GatherResult<DigitalIntervalSeries>::create(src, ivals);
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
