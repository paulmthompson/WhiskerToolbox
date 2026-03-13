
#include "GatherPipelineExecutor.hpp"

#include "GatherResult/GatherResult.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TypeTraits/DataTypeTraits.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/extension/RangeReductionTypes.hpp"

#include <cmath>     // NAN
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

namespace WhiskerToolbox::Gather {

namespace {

/**
 * @brief Convert interval boundaries to the source data's TimeFrame coordinate
 *        system if the source and interval series use different TimeFrames.
 *
 * If either TimeFrame is null, or they are the same object, the original
 * interval series shared_ptr is returned unchanged (zero allocation).
 *
 * When conversion is required, a new DigitalIntervalSeries is constructed with
 * the converted boundaries and the source's TimeFrame attached.
 *
 * @param intervals The row interval series (in its own coordinate system)
 * @param source_tf The source data's TimeFrame
 * @return Shared pointer to intervals expressed in the source's coordinate system
 */
std::shared_ptr<DigitalIntervalSeries> prepareIntervalsForGather(
        std::shared_ptr<DigitalIntervalSeries> const & intervals,
        std::shared_ptr<TimeFrame> const & source_tf) {
    auto interval_tf = intervals->getTimeFrame();

    // No conversion needed: same TimeFrame or one of them is null
    if (!source_tf || !interval_tf || source_tf.get() == interval_tf.get()) {
        return intervals;
    }

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

}// namespace

// ============================================================================
// extractSingleFloat
// ============================================================================

float extractSingleFloat(DataTypeVariant const & output) {
    return std::visit([](auto const & ptr) -> float {
        using T = std::remove_reference_t<decltype(*ptr)>;

        if constexpr (std::is_same_v<T, AnalogTimeSeries>) {
            auto values = ptr->getAnalogTimeSeries();
            if (values.empty()) {
                return NAN;
            }
            return values[0];

        } else if constexpr (std::is_same_v<T, RaggedAnalogTimeSeries>) {
            auto time_indices = ptr->getTimeIndices();
            if (time_indices.empty()) {
                return NAN;
            }
            auto data = ptr->getDataAtTime(time_indices[0]);
            return data.empty() ? NAN : data[0];

        } else {
            throw std::runtime_error(
                    "extractSingleFloat: pipeline output is not a float time series "
                    "(AnalogTimeSeries or RaggedAnalogTimeSeries). "
                    "Ensure the pipeline ends with a range reduction.");
        }
    },
                      output);
}

// ============================================================================
// applyRangeReductionToOutput — apply terminal range reduction to pipeline output
// ============================================================================

namespace {

/**
 * @brief Cast the result of executeErased to float, handling int results too.
 */
float castReductionResult(std::any const & result) {
    if (auto * f = std::any_cast<float>(&result)) return *f;
    if (auto * d = std::any_cast<double>(&result)) return static_cast<float>(*d);
    if (auto * i = std::any_cast<int>(&result)) return static_cast<float>(*i);
    if (auto * l = std::any_cast<long>(&result)) return static_cast<float>(*l);
    if (auto * ll = std::any_cast<long long>(&result)) return static_cast<float>(*ll);
    if (auto * u = std::any_cast<unsigned>(&result)) return static_cast<float>(*u);
    if (auto * sz = std::any_cast<std::size_t>(&result)) return static_cast<float>(*sz);
    throw std::runtime_error("castReductionResult: unsupported reduction output type");
}

/**
 * @brief Apply a pipeline's terminal range reduction to the element-transformed
 *        output, producing a single float scalar.
 *
 * The pipeline's element transforms have already been applied (via executePipeline),
 * producing a DataTypeVariant containing the transformed data. This function
 * applies the range reduction to that output.
 *
 * Only AnalogTimeSeries output is currently supported (element type = TimeValuePoint).
 */
float applyRangeReductionToOutput(
        DataTypeVariant const & output,
        WhiskerToolbox::Transforms::V2::TransformPipeline const & pipeline) {
    if (!pipeline.hasRangeReduction()) {
        return extractSingleFloat(output);
    }

    auto const & reduction = pipeline.getRangeReduction().value();
    auto & registry = WhiskerToolbox::Transforms::V2::RangeReductionRegistry::instance();

    return std::visit([&](auto const & ptr) -> float {
        using T = std::remove_reference_t<decltype(*ptr)>;

        if constexpr (std::is_same_v<T, AnalogTimeSeries>) {
            using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;
            auto view = ptr->view();
            std::vector<TimeValuePoint> elements(view.begin(), view.end());
            std::span<TimeValuePoint const> span{elements};
            std::any input_any{span};

            std::any params_to_use = reduction.params.has_value()
                ? reduction.params
                : std::any{WhiskerToolbox::Transforms::V2::NoReductionParams{}};

            std::any result = registry.executeErased(
                    reduction.reduction_name,
                    typeid(TimeValuePoint),
                    input_any,
                    params_to_use);
            return castReductionResult(result);

        } else if constexpr (std::is_same_v<T, RaggedAnalogTimeSeries>) {
            // For ragged output, flatten to a single span and reduce
            // This is a best-effort approach; most pipelines produce
            // non-ragged AnalogTimeSeries after element transforms.
            using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;
            std::vector<TimeValuePoint> elements;
            for (auto const & ti : ptr->getTimeIndices()) {
                auto data = ptr->getDataAtTime(ti);
                for (auto val : data) {
                    elements.push_back({ti, val});
                }
            }
            if (elements.empty()) return NAN;
            std::span<TimeValuePoint const> span{elements};
            std::any input_any{span};

            std::any params_to_use = reduction.params.has_value()
                ? reduction.params
                : std::any{WhiskerToolbox::Transforms::V2::NoReductionParams{}};

            std::any result = registry.executeErased(
                    reduction.reduction_name,
                    typeid(TimeValuePoint),
                    input_any,
                    params_to_use);
            return castReductionResult(result);

        } else if constexpr (std::is_same_v<T, DigitalEventSeries>) {
            // Materialize events into a vector and apply reduction.
            std::vector<EventWithId> elements;
            for (auto const & e : ptr->view()) {
                elements.push_back(e);
            }
            std::span<EventWithId const> span{elements};
            std::any input_any{span};

            std::any params_to_use = reduction.params.has_value()
                ? reduction.params
                : std::any{WhiskerToolbox::Transforms::V2::NoReductionParams{}};

            std::any result = registry.executeErased(
                    reduction.reduction_name,
                    typeid(EventWithId),
                    input_any,
                    params_to_use);
            return castReductionResult(result);

        } else if constexpr (std::is_same_v<T, DigitalIntervalSeries>) {
            // Materialize intervals into a vector and apply reduction.
            std::vector<IntervalWithId> elements;
            for (auto const & iv : ptr->view()) {
                elements.push_back(iv);
            }
            std::span<IntervalWithId const> span{elements};
            std::any input_any{span};

            std::any params_to_use = reduction.params.has_value()
                ? reduction.params
                : std::any{WhiskerToolbox::Transforms::V2::NoReductionParams{}};

            std::any result = registry.executeErased(
                    reduction.reduction_name,
                    typeid(IntervalWithId),
                    input_any,
                    params_to_use);
            return castReductionResult(result);

        } else {
            throw std::runtime_error(
                    "applyRangeReductionToOutput: pipeline output is not a float "
                    "time series — cannot apply range reduction");
        }
    }, output);
}

} // anonymous namespace

// ============================================================================
// gatherAndExecutePipeline
// ============================================================================

std::vector<float> gatherAndExecutePipeline(
        DataTypeVariant const & source,
        std::shared_ptr<DigitalIntervalSeries> intervals,
        WhiskerToolbox::Transforms::V2::TransformPipeline const & pipeline) {
    using WhiskerToolbox::Transforms::V2::executePipeline;

    return std::visit([&](auto const & ptr) -> std::vector<float> {
        using T = std::remove_reference_t<decltype(*ptr)>;

        // RaggedAnalogTimeSeries is excluded: GatherResult has no element_type_of
        // specialisation for it and per-interval gather semantics are ill-defined
        // for a container that already has multiple values per time point.
        // Gather is supported for types with DataTraits (AnalogTimeSeries,
        // MaskData, PointData, LineData) plus DigitalEventSeries and
        // DigitalIntervalSeries (which have element_type_of but not DataTraits).
        // RaggedAnalogTimeSeries is excluded — its per-interval gather semantics
        // are ill-defined.
        constexpr bool can_gather =
            (TypeTraits::HasDataTraits<T> && !std::is_same_v<T, RaggedAnalogTimeSeries>) ||
            std::is_same_v<T, DigitalEventSeries> ||
            std::is_same_v<T, DigitalIntervalSeries>;

        if constexpr (can_gather) {
            // Convert interval boundaries to the source's TimeFrame if needed
            auto gather_ivals = prepareIntervalsForGather(intervals, ptr->getTimeFrame());

            // Gather: create one view/copy per interval
            auto gather = GatherResult<T>::create(ptr, gather_ivals);

            std::vector<float> result;
            result.reserve(gather.size());

            bool const has_element_steps = !pipeline.empty();

            for (std::size_t i = 0; i < gather.size(); ++i) {
                DataTypeVariant segment_var{gather[i]};

                if (has_element_steps) {
                    // Run element transforms then apply range reduction
                    DataTypeVariant output = executePipeline(segment_var, pipeline);
                    result.push_back(applyRangeReductionToOutput(output, pipeline));
                } else {
                    // No element steps — apply range reduction directly to
                    // the raw gathered segment (avoids "Pipeline has no steps")
                    result.push_back(applyRangeReductionToOutput(segment_var, pipeline));
                }
            }

            return result;

        } else {
            throw std::runtime_error(
                    "gatherAndExecutePipeline: source type does not support gather+pipeline. "
                    "RaggedAnalogTimeSeries is not currently supported (no element_type_of "
                    "specialisation). Other types must satisfy HasDataTraits or be "
                    "DigitalEventSeries/DigitalIntervalSeries.");
        }
    },
                      source);
}

}// namespace WhiskerToolbox::Gather
