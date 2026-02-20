
#include "GatherPipelineExecutor.hpp"

#include "GatherResult/GatherResult.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/TypeTraits/DataTypeTraits.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"

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
        if constexpr (TypeTraits::HasDataTraits<T> && !std::is_same_v<T, RaggedAnalogTimeSeries>) {
            // Convert interval boundaries to the source's TimeFrame if needed
            auto gather_ivals = prepareIntervalsForGather(intervals, ptr->getTimeFrame());

            // Gather: create one view/copy per interval
            auto gather = GatherResult<T>::create(ptr, gather_ivals);

            std::vector<float> result;
            result.reserve(gather.size());

            for (std::size_t i = 0; i < gather.size(); ++i) {
                // Wrap the gathered view in a DataTypeVariant for type-erased
                // pipeline dispatch — avoids instantiating execute<T>() here,
                // relying on the explicit instantiations in TransformPipeline.cpp.
                DataTypeVariant segment_var{gather[i]};
                DataTypeVariant output = executePipeline(segment_var, pipeline);
                result.push_back(extractSingleFloat(output));
            }

            return result;

        } else {
            throw std::runtime_error(
                    "gatherAndExecutePipeline: source type does not support gather+pipeline. "
                    "RaggedAnalogTimeSeries is not currently supported (no element_type_of "
                    "specialisation). Other types must satisfy TypeTraits::HasDataTraits.");
        }
    },
                      source);
}

}// namespace WhiskerToolbox::Gather
