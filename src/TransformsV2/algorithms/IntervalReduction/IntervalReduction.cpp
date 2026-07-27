#include "IntervalReduction.hpp"

#include "core/ComputeContext.hpp"
#include "core/RangeReductionRegistry.hpp"
#include "extension/RangeReductionTypes.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "GatherResult/GatherResult.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <cmath>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace Neuralyzer::Transforms::V2 {

namespace {

// ============================================================================
// Shared Helpers
// ============================================================================

/**
 * @brief Ensure params is a valid std::any for the reduction registry.
 *
 * If the JSON is empty or "{}", returns NoReductionParams{}.
 */
std::any ensureReductionParams(std::string const & json) {
    if (json.empty() || json == "{}") {
        return std::any{NoReductionParams{}};
    }
    // For now, only stateless reductions are supported via this path.
    // Parameterized reductions would require looking up the reduction's
    // parameter type and deserializing accordingly.
    return std::any{NoReductionParams{}};
}

/**
 * @brief Cast the result of executeErased to float, handling various numeric types.
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
    throw std::runtime_error("IntervalReduction: unsupported reduction output type");
}

/**
 * @brief Build TimeFrameInterval vector from DigitalIntervalSeries view.
 */
std::vector<TimeFrameInterval> buildTimeFrameIntervals(
        DigitalIntervalSeries const & intervals) {
    auto const & interval_data = intervals.view();
    std::vector<TimeFrameInterval> tf_intervals;
    tf_intervals.reserve(interval_data.size());
    for (auto const & interval: interval_data) {
        tf_intervals.push_back(TimeFrameInterval{
                TimeFrameIndex(interval.value().start),
                TimeFrameIndex(interval.value().end)});
    }
    return tf_intervals;
}

/**
 * @brief Get the TimeFrame from an interval series, creating a default if null.
 *
 * TensorData::createFromIntervals requires a non-null TimeFrame. If the
 * interval series has no TimeFrame set (e.g., from test builders), we
 * create a minimal default.
 */
std::shared_ptr<TimeFrame> getOrCreateTimeFrame(
        DigitalIntervalSeries const & intervals) {
    auto tf = intervals.getTimeFrame();
    if (tf) {
        return tf;
    }
    // Create a default TimeFrame — identity mapping
    return std::make_shared<TimeFrame>();
}

/**
 * @brief Create a non-owning source shared_ptr alias for use with GatherResult.
 *
 * The resulting shared_ptr does NOT own the object; it merely provides a
 * compatible smart-pointer interface. The caller must ensure the referenced
 * object outlives the shared_ptr (which is guaranteed within a single
 * transform function call).
 *
 * Current DataObject view APIs used by GatherResult require non-const source
 * pointers, so this source alias remains mutable until those APIs are
 * const-correct.
 */
template<typename T>
std::shared_ptr<T> borrowSourceAsShared(T const & obj) {
    // Aliasing constructor with null controlling block → no-op deleter
    return std::shared_ptr<T>(
            std::shared_ptr<T>{},
            const_cast<T *>(&obj));
}

/**
 * @brief Create a non-owning const shared_ptr alias for row-defining intervals.
 */
std::shared_ptr<DigitalIntervalSeries const> borrowIntervalsAsShared(
        DigitalIntervalSeries const & intervals) {
    return {
            std::shared_ptr<DigitalIntervalSeries const>{},
            &intervals};
}

}// anonymous namespace

// ============================================================================
// AnalogIntervalReduction
// ============================================================================

std::shared_ptr<TensorData> analogIntervalReduction(
        DigitalIntervalSeries const & intervals,
        AnalogTimeSeries const & analog,
        IntervalReductionParams const & params,
        ComputeContext const & ctx) {

    ctx.reportProgress(0);

    if (ctx.is_cancelled && ctx.is_cancelled()) {
        return std::make_shared<TensorData>();
    }

    auto const & interval_data = intervals.view();
    if (interval_data.empty()) {
        ctx.reportProgress(100);
        return std::make_shared<TensorData>();
    }

    // Build interval list for RowDescriptor
    auto tf_intervals = buildTimeFrameIntervals(intervals);

    ctx.reportProgress(5);

    // Resolve range reduction
    auto & registry = RangeReductionRegistry::instance();
    std::string const reduction_name = params.reduction_name;
    std::any reduction_params = ensureReductionParams(params.reduction_params_json);

    using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;

    // GatherResult converts interval bounds to the source TimeFrame at query time.
    auto analog_ptr = borrowSourceAsShared(analog);
    auto gather = GatherResult<AnalogTimeSeries>::create(
            analog_ptr,
            borrowIntervalsAsShared(intervals));

    ctx.reportProgress(15);

    // Build reducer factory
    Neuralyzer::Gather::ReducerFactoryV2<TimeValuePoint, float> const factory =
            [&reduction_name, &reduction_params](
                    PipelineValueStore const &) -> Neuralyzer::Gather::ReducerFn<TimeValuePoint, float> {
        return [&reduction_name, &reduction_params](
                       std::span<TimeValuePoint const> input) -> float {
            auto & reg = RangeReductionRegistry::instance();
            std::any const input_any{input};
            std::any const result = reg.executeErased(
                    reduction_name, typeid(TimeValuePoint), input_any, reduction_params);
            return castReductionResult(result);
        };
    };

    // Reduce all gathered views
    auto values = gather.template reduce<float>(factory);

    ctx.reportProgress(85);

    // Build output TensorData
    auto const num_rows = values.size();
    auto time_frame = getOrCreateTimeFrame(intervals);

    auto result = std::make_shared<TensorData>(
            TensorData::createFromIntervals(
                    values,
                    num_rows,
                    1,
                    std::move(tf_intervals),
                    time_frame,
                    {params.column_name}));

    ctx.reportProgress(100);
    return result;
}

// ============================================================================
// EventIntervalReduction
// ============================================================================

std::shared_ptr<TensorData> eventIntervalReduction(
        DigitalIntervalSeries const & intervals,
        DigitalEventSeries const & events,
        IntervalReductionParams const & params,
        ComputeContext const & ctx) {

    ctx.reportProgress(0);

    if (ctx.is_cancelled && ctx.is_cancelled()) {
        return std::make_shared<TensorData>();
    }

    auto const & interval_data = intervals.view();
    if (interval_data.empty()) {
        ctx.reportProgress(100);
        return std::make_shared<TensorData>();
    }

    // Build interval list for RowDescriptor
    auto tf_intervals = buildTimeFrameIntervals(intervals);

    ctx.reportProgress(5);

    // Resolve range reduction
    auto & registry = RangeReductionRegistry::instance();
    std::string const reduction_name = params.reduction_name;
    std::any reduction_params = ensureReductionParams(params.reduction_params_json);

    using EventElement = Neuralyzer::Gather::element_type_of_t<DigitalEventSeries>;

    // GatherResult converts interval bounds to the source TimeFrame at query time.
    auto events_ptr = borrowSourceAsShared(events);
    auto gather = GatherResult<DigitalEventSeries>::create(
            events_ptr,
            borrowIntervalsAsShared(intervals));

    ctx.reportProgress(15);

    // Build reducer factory
    Neuralyzer::Gather::ReducerFactoryV2<EventElement, float> const factory =
            [&reduction_name, &reduction_params](
                    PipelineValueStore const &) -> Neuralyzer::Gather::ReducerFn<EventElement, float> {
        return [&reduction_name, &reduction_params](
                       std::span<EventElement const> input) -> float {
            auto & reg = RangeReductionRegistry::instance();
            std::any const input_any{input};
            std::any const result = reg.executeErased(
                    reduction_name, typeid(EventElement), input_any, reduction_params);
            return castReductionResult(result);
        };
    };

    // Reduce all gathered views
    auto values = gather.template reduce<float>(factory);

    ctx.reportProgress(85);

    // Build output TensorData
    auto const num_rows = values.size();
    auto time_frame = getOrCreateTimeFrame(intervals);

    auto result = std::make_shared<TensorData>(
            TensorData::createFromIntervals(
                    values,
                    num_rows,
                    1,
                    std::move(tf_intervals),
                    time_frame,
                    {params.column_name}));

    ctx.reportProgress(100);
    return result;
}

// ============================================================================
// IntervalOverlapReduction
// ============================================================================

// The two interval-series parameters intentionally distinguish row windows from
// the source interval series gathered inside each row.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
std::shared_ptr<TensorData> intervalOverlapReduction(
        DigitalIntervalSeries const & intervals,
        DigitalIntervalSeries const & source,
        IntervalReductionParams const & params,
        ComputeContext const & ctx) {

    ctx.reportProgress(0);

    if (ctx.is_cancelled && ctx.is_cancelled()) {
        return std::make_shared<TensorData>();
    }

    auto const & interval_data = intervals.view();
    if (interval_data.empty()) {
        ctx.reportProgress(100);
        return std::make_shared<TensorData>();
    }

    // Build interval list for RowDescriptor
    auto tf_intervals = buildTimeFrameIntervals(intervals);

    ctx.reportProgress(5);

    // Resolve range reduction
    auto & registry = RangeReductionRegistry::instance();
    std::string const reduction_name = params.reduction_name;
    std::any reduction_params = ensureReductionParams(params.reduction_params_json);

    using IntervalElement = Neuralyzer::Gather::element_type_of_t<DigitalIntervalSeries>;

    // GatherResult converts interval bounds to the source TimeFrame at query time.
    auto source_ptr = borrowSourceAsShared(source);
    auto gather = GatherResult<DigitalIntervalSeries>::create(
            source_ptr,
            borrowIntervalsAsShared(intervals));

    ctx.reportProgress(15);

    // Build reducer factory
    Neuralyzer::Gather::ReducerFactoryV2<IntervalElement, float> const factory =
            [&reduction_name, &reduction_params](
                    PipelineValueStore const &) -> Neuralyzer::Gather::ReducerFn<IntervalElement, float> {
        return [&reduction_name, &reduction_params](
                       std::span<IntervalElement const> input) -> float {
            auto & reg = RangeReductionRegistry::instance();
            std::any const input_any{input};
            std::any const result = reg.executeErased(
                    reduction_name, typeid(IntervalElement), input_any, reduction_params);
            return castReductionResult(result);
        };
    };

    // Reduce all gathered views
    auto values = gather.template reduce<float>(factory);

    ctx.reportProgress(85);

    // Build output TensorData
    auto const num_rows = values.size();
    auto time_frame = getOrCreateTimeFrame(intervals);

    auto result = std::make_shared<TensorData>(
            TensorData::createFromIntervals(
                    values,
                    num_rows,
                    1,
                    std::move(tf_intervals),
                    time_frame,
                    {params.column_name}));

    ctx.reportProgress(100);
    return result;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

}// namespace Neuralyzer::Transforms::V2
