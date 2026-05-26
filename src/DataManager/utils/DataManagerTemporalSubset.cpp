/// @file DataManagerTemporalSubset.cpp
/// @brief Implementation of type-erased temporal subset utilities.

#include "DataManagerTemporalSubset.hpp"

#include "DataManager/DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Points/Point_Data.hpp"
#include "RaggedTimeSeries/RaggedTimeSeries.hpp"
#include "Tensors/TensorData.hpp"

#include <spdlog/spdlog.h>

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

template<typename TSeries, typename TData>
[[nodiscard]] std::shared_ptr<TSeries>
createRaggedTemporalSubset(std::shared_ptr<TSeries const> const & source,
                           TimeFrameInterval const & interval) {
    using EntryTuple = std::tuple<TimeFrameIndex,
                                  EntityId,
                                  std::reference_wrapper<TData const>>;

    std::vector<EntryTuple> entries;
    for (auto const & element: source->getElementsInRange(interval)) {
        entries.emplace_back(
                std::get<0>(element),
                std::get<1>(element),
                std::get<2>(element));
    }

    auto const subset = RaggedTimeSeries<TData>::createFromView(
            std::move(entries),
            source->getTimeFrame(),
            source->getImageSize());
    return std::static_pointer_cast<TSeries>(subset);
}

[[nodiscard]] std::shared_ptr<RaggedAnalogTimeSeries>
createRaggedAnalogTemporalSubset(std::shared_ptr<RaggedAnalogTimeSeries const> const & source,
                                 TimeFrameInterval const & interval) {
    std::vector<std::pair<TimeFrameIndex, float>> entries;
    for (auto const & [time, value]: source->elements()) {
        if (time >= interval.start && time <= interval.end) {
            entries.emplace_back(time, value);
        }
    }

    return RaggedAnalogTimeSeries::createFromView(
            std::move(entries),
            source->getTimeFrame());
}

template<typename T>
[[nodiscard]] constexpr bool isUnsupportedTemporalSubsetType() {
    return std::is_same_v<T, MediaData> || std::is_same_v<T, TensorData>;
}

[[nodiscard]] std::optional<DataTypeVariant>
createTemporalSubsetImpl(DataTypeVariant const & source,
                         TimeFrameInterval const & interval,
                         std::string & error_message) {
    return std::visit(
            [&](auto const & ptr) -> std::optional<DataTypeVariant> {
                using T = std::decay_t<decltype(*ptr)>;

                if (!ptr) {
                    error_message = "createTemporalSubset: source holds a null shared_ptr";
                    spdlog::warn("{}", error_message);
                    return std::nullopt;
                }

                if constexpr (isUnsupportedTemporalSubsetType<T>()) {
                    error_message =
                            "createTemporalSubset: temporal subset is not supported for " +
                            std::string(typeid(T).name()) + " in v1";
                    spdlog::warn("{}", error_message);
                    return std::nullopt;
                } else if constexpr (std::is_same_v<T, AnalogTimeSeries>) {
                    return AnalogTimeSeries::createView(ptr, interval.start, interval.end);
                } else if constexpr (std::is_same_v<T, DigitalEventSeries>) {
                    return DigitalEventSeries::createView(ptr, interval.start, interval.end);
                } else if constexpr (std::is_same_v<T, DigitalIntervalSeries>) {
                    return DigitalIntervalSeries::createView(
                            ptr,
                            interval.start.getValue(),
                            interval.end.getValue());
                } else if constexpr (std::is_same_v<T, MaskData>) {
                    return createRaggedTemporalSubset<MaskData, Mask2D>(ptr, interval);
                } else if constexpr (std::is_same_v<T, LineData>) {
                    return createRaggedTemporalSubset<LineData, Line2D>(ptr, interval);
                } else if constexpr (std::is_same_v<T, PointData>) {
                    return createRaggedTemporalSubset<PointData, Point2D<float>>(ptr, interval);
                } else if constexpr (std::is_same_v<T, RaggedAnalogTimeSeries>) {
                    return createRaggedAnalogTemporalSubset(ptr, interval);
                } else {
                    error_message =
                            "createTemporalSubset: unhandled data type " +
                            std::string(typeid(T).name());
                    spdlog::warn("{}", error_message);
                    return std::nullopt;
                }
            },
            source);
}

}// namespace

std::optional<DataTypeVariant>
createTemporalSubset(DataTypeVariant const & source,
                     TimeFrameIndex time,
                     std::string & error_message) {
    return createTemporalSubset(source, TimeFrameInterval{time, time}, error_message);
}

std::optional<DataTypeVariant>
createTemporalSubset(DataTypeVariant const & source,
                     TimeFrameInterval interval,
                     std::string & error_message) {
    if (interval.start > interval.end) {
        error_message = "createTemporalSubset: interval.start must be <= interval.end";
        spdlog::warn("{}", error_message);
        return std::nullopt;
    }

    return createTemporalSubsetImpl(source, interval, error_message);
}

std::optional<DataTypeVariant>
createTemporalSubset(DataManager & dm,
                     std::string const & key,
                     TimeFrameInterval interval,
                     std::string & error_message) {
    auto const variant = dm.getDataVariant(key);
    if (!variant.has_value()) {
        error_message = "createTemporalSubset: no data at key '" + key + "'";
        spdlog::warn("{}", error_message);
        return std::nullopt;
    }

    return createTemporalSubset(variant.value(), interval, error_message);
}
