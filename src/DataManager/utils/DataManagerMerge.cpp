/// @file DataManagerMerge.cpp
/// @brief Implementation of type-erased overwrite-merge utilities.

#include "DataManagerMerge.hpp"

#include "DataManager/DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/Point_Data.hpp"
#include "RaggedTimeSeries/RaggedTimeSeries.hpp"
#include "Tensors/TensorData.hpp"

#include <spdlog/spdlog.h>

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

[[nodiscard]] std::optional<std::size_t>
mergeOverwriteDataImpl(DataManager & dm,
                       std::string const & target_key,
                       DataTypeVariant const & src_var,
                       std::optional<std::string_view> const source_key_for_diagnostics,
                       std::string & error_message) {
    auto const dst_var = dm.getDataVariant(target_key);
    if (!dst_var.has_value()) {
        error_message = "mergeOverwriteData: no data at target key '" + target_key + "'";
        spdlog::warn("{}", error_message);
        return std::nullopt;
    }

    if (src_var.index() != dst_var->index()) {
        error_message = "mergeOverwriteData: source and target have incompatible types";
        spdlog::warn("{}", error_message);
        return std::nullopt;
    }

    return std::visit(
            [&](auto const & src_ptr) -> std::optional<std::size_t> {
                using DataT = std::decay_t<decltype(*src_ptr)>;
                auto const dst_ptr = dm.getData<DataT>(target_key);

                if (!src_ptr || !dst_ptr) {
                    error_message = "mergeOverwriteData: source or target holds a null shared_ptr";
                    spdlog::warn("{}", error_message);
                    return std::nullopt;
                }

                if (src_ptr == dst_ptr) {
                    error_message = "mergeOverwriteData: cannot merge a data object into itself";
                    spdlog::warn("{}", error_message);
                    return std::nullopt;
                }

                if constexpr (requires(DataT const & source, DataT & target) {
                                  {
                                      source.mergeOverwriteTo(target, NotifyObservers::Yes)
                                  } -> std::same_as<std::optional<std::size_t>>;
                              }) {
                    auto const source_time_frame = src_ptr->getTimeFrame();
                    auto const target_time_frame = dst_ptr->getTimeFrame();
                    if (!source_time_frame || !target_time_frame ||
                        source_time_frame != target_time_frame) {
                        error_message =
                                "mergeOverwriteData: source and target must share the same "
                                "TimeFrame object";
                        spdlog::warn("{}", error_message);
                        return std::nullopt;
                    }

                    if (source_key_for_diagnostics.has_value()) {
                        auto const source_time_key =
                                dm.getTimeKey(std::string(source_key_for_diagnostics.value()));
                        auto const target_time_key = dm.getTimeKey(target_key);
                        if (source_time_key != target_time_key) {
                            spdlog::warn(
                                    "mergeOverwriteData: TimeKey mismatch for '{}' ({}) and "
                                    "'{}' ({}) despite shared TimeFrame",
                                    source_key_for_diagnostics.value(),
                                    source_time_key.str(),
                                    target_key,
                                    target_time_key.str());
                        }
                    }

                    auto const merged =
                            src_ptr->mergeOverwriteTo(*dst_ptr, NotifyObservers::Yes);
                    if (!merged.has_value()) {
                        error_message =
                                "mergeOverwriteData: mergeOverwriteTo failed precondition checks";
                        spdlog::warn("{}", error_message);
                    }
                    return merged;
                } else {
                    error_message =
                            "mergeOverwriteData: overwrite merge is not supported for " +
                            std::string(typeid(DataT).name());
                    spdlog::warn("{}", error_message);
                    return std::nullopt;
                }
            },
            src_var);
}

}// namespace

bool supportsMergeOverwrite(DataTypeVariant const & source) {
    return std::visit(
            [](auto const & src_ptr) -> bool {
                if (!src_ptr) {
                    return false;
                }
                using DataT = std::decay_t<decltype(*src_ptr)>;
                return requires(DataT const & source, DataT & target) {
                    {
                        source.mergeOverwriteTo(target, NotifyObservers::Yes)
                    } -> std::same_as<std::optional<std::size_t>>;
                };
            },
            source);
}

std::optional<std::size_t>
mergeOverwriteData(DataManager & dm,
                   std::string const & target_key,
                   std::string const & source_key,
                   std::string & error_message) {
    if (target_key == source_key) {
        error_message = "mergeOverwriteData: target_key and source_key must differ";
        spdlog::warn("{}", error_message);
        return std::nullopt;
    }

    auto const src_var = dm.getDataVariant(source_key);
    if (!src_var.has_value()) {
        error_message = "mergeOverwriteData: no data at source key '" + source_key + "'";
        spdlog::warn("{}", error_message);
        return std::nullopt;
    }

    return mergeOverwriteDataImpl(
            dm, target_key, src_var.value(), source_key, error_message);
}

std::optional<std::size_t>
mergeOverwriteData(DataManager & dm,
                   std::string const & target_key,
                   DataTypeVariant const & source,
                   std::string & error_message) {
    return mergeOverwriteDataImpl(dm, target_key, source, std::nullopt, error_message);
}
