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
#include <iostream>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

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
    for (auto const & interval : interval_data) {
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
 * @brief Create a non-owning shared_ptr alias for use with GatherResult.
 *
 * The resulting shared_ptr does NOT own the object; it merely provides a
 * compatible smart-pointer interface. The caller must ensure the referenced
 * object outlives the shared_ptr (which is guaranteed within a single
 * transform function call).
 */
template<typename T>
std::shared_ptr<T> borrowAsShared(T const & obj) {
    // Aliasing constructor with null controlling block → no-op deleter
    return std::shared_ptr<T>(
            std::shared_ptr<T>{},
            const_cast<T *>(&obj));
}

/**
 * @brief Prepare intervals for gathering, with cross-TimeFrame conversion if needed.
 *
 * If the source data and interval series have different TimeFrames, the interval
 * boundaries are converted from the interval's coordinate system to the source's
 * coordinate system so that GatherResult queries the correct data range.
 *
 * When no conversion is needed (same TimeFrame, or either is null), a non-owning
 * alias of the original intervals is returned to avoid unnecessary copies.
 *
 * @param intervals The original interval series (in its own TimeFrame)
 * @param source_tf The source data's TimeFrame
 * @return Shared pointer to intervals with boundaries in the source's TimeFrame
 */
std::shared_ptr<DigitalIntervalSeries> prepareIntervalsForGather(
        DigitalIntervalSeries const & intervals,
        std::shared_ptr<TimeFrame> const & source_tf) {

    auto interval_tf = intervals.getTimeFrame();

    // No conversion needed if same TimeFrame or either is null
    if (!source_tf || !interval_tf || source_tf.get() == interval_tf.get()) {
        return borrowAsShared(intervals);
    }

    // Convert interval boundaries from interval's TimeFrame to source's TimeFrame
    auto const & interval_data = intervals.view();
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
    std::string const reduction_name = params.getReductionName();
    std::any reduction_params = ensureReductionParams(params.getReductionParamsJson());

    using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;

    // Create GatherResult with cross-TimeFrame conversion if needed
    auto analog_ptr = borrowAsShared(analog);
    auto gather_intervals = prepareIntervalsForGather(intervals, analog.getTimeFrame());
    auto gather = GatherResult<AnalogTimeSeries>::create(analog_ptr, gather_intervals);

    ctx.reportProgress(15);

    // Build reducer factory
    ReducerFactoryV2<TimeValuePoint, float> factory =
            [&reduction_name, &reduction_params](
                    PipelineValueStore const &) -> ReducerFn<TimeValuePoint, float> {
        return [&reduction_name, &reduction_params](
                       std::span<TimeValuePoint const> input) -> float {
            auto & reg = RangeReductionRegistry::instance();
            std::any input_any{input};
            std::any result = reg.executeErased(
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
                    {params.getColumnName()}));

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
    std::string const reduction_name = params.getReductionName();
    std::any reduction_params = ensureReductionParams(params.getReductionParamsJson());

    using EventElement = WhiskerToolbox::Gather::element_type_of_t<DigitalEventSeries>;

    // Create GatherResult with cross-TimeFrame conversion if needed
    auto events_ptr = borrowAsShared(events);
    auto gather_intervals = prepareIntervalsForGather(intervals, events.getTimeFrame());
    auto gather = GatherResult<DigitalEventSeries>::create(events_ptr, gather_intervals);

    ctx.reportProgress(15);

    // Build reducer factory
    ReducerFactoryV2<EventElement, float> factory =
            [&reduction_name, &reduction_params](
                    PipelineValueStore const &) -> ReducerFn<EventElement, float> {
        return [&reduction_name, &reduction_params](
                       std::span<EventElement const> input) -> float {
            auto & reg = RangeReductionRegistry::instance();
            std::any input_any{input};
            std::any result = reg.executeErased(
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
                    {params.getColumnName()}));

    ctx.reportProgress(100);
    return result;
}

// ============================================================================
// IntervalOverlapReduction
// ============================================================================

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
    std::string const reduction_name = params.getReductionName();
    std::any reduction_params = ensureReductionParams(params.getReductionParamsJson());

    using IntervalElement = WhiskerToolbox::Gather::element_type_of_t<DigitalIntervalSeries>;

    // Create GatherResult with cross-TimeFrame conversion if needed
    auto source_ptr = borrowAsShared(source);
    auto gather_intervals = prepareIntervalsForGather(intervals, source.getTimeFrame());
    auto gather = GatherResult<DigitalIntervalSeries>::create(source_ptr, gather_intervals);

    ctx.reportProgress(15);

    // Build reducer factory
    ReducerFactoryV2<IntervalElement, float> factory =
            [&reduction_name, &reduction_params](
                    PipelineValueStore const &) -> ReducerFn<IntervalElement, float> {
        return [&reduction_name, &reduction_params](
                       std::span<IntervalElement const> input) -> float {
            auto & reg = RangeReductionRegistry::instance();
            std::any input_any{input};
            std::any result = reg.executeErased(
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
                    {params.getColumnName()}));

    ctx.reportProgress(100);
    return result;
}

}// namespace WhiskerToolbox::Transforms::V2
