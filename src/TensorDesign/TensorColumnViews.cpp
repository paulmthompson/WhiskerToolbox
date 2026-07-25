/**
 * @file TensorColumnViews.cpp
 * @brief Implementation of TensorData ↔ AnalogTimeSeries bridge utilities.
 */

#include "TensorColumnViews.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "Tensors/TensorData.hpp"
#include "TransformsV2/algorithms/AnalogToTensor/AnalogToTensor.hpp"
#include "TransformsV2/algorithms/TensorToAnalog/TensorToAnalog.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace Neuralyzer::TensorDesign {

// NOLINTBEGIN(bugprone-easily-swappable-parameters) — tensor_key and prefix are distinct DM key roles
std::size_t createTensorColumnViews(
        DataManager & dm,
        std::string const & tensor_key,
        std::string const & prefix,
        std::vector<int> const & columns) {
    auto tensor = dm.getData<TensorData>(tensor_key);
    if (!tensor) {
        spdlog::error(
                "TensorColumnViews: no TensorData at key '{}'", tensor_key);
        return 0;
    }

    if (tensor->ndim() != 2) {
        spdlog::error(
                "TensorColumnViews: TensorData at '{}' is {}D, expected 2D",
                tensor_key,
                tensor->ndim());
        return 0;
    }

    using namespace Neuralyzer::Transforms::V2;
    using namespace Neuralyzer::Transforms::V2::Examples;

    TensorToAnalogParams params;
    params.columns = columns;

    ComputeContext const ctx;
    auto views = tensorToAnalog(*tensor, params, ctx);
    if (views.empty()) {
        return 0;
    }

    auto const & col_names = tensor->columnNames();
    auto const time_key = dm.getTimeKey(tensor_key);

    std::vector<int> actual_columns = columns;
    if (actual_columns.empty()) {
        actual_columns.resize(tensor->numColumns());
        std::iota(actual_columns.begin(), actual_columns.end(), 0);
    }

    std::size_t count = 0;
    for (std::size_t i = 0; i < views.size(); ++i) {
        auto const col_idx = static_cast<std::size_t>(actual_columns[i]);

        std::string suffix;
        if (col_idx < col_names.size() && !col_names[col_idx].empty()) {
            suffix = col_names[col_idx];
        } else {
            suffix = "ch" + std::to_string(col_idx);
        }

        auto key = prefix;
        key += "/";
        key += suffix;
        dm.setData<AnalogTimeSeries>(key, views[i], time_key);
        ++count;
    }

    return count;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

std::vector<AnalogKeyGroup> discoverAnalogKeyGroups(DataManager & dm) {
    auto const analog_keys = dm.getKeys<AnalogTimeSeries>();

    std::map<std::string, std::vector<std::string>> groups;
    for (auto const & key: analog_keys) {
        auto const pos = key.rfind('_');
        if (pos == std::string::npos || pos == 0) {
            continue;
        }
        auto const group_prefix = key.substr(0, pos);
        groups[group_prefix].push_back(key);
    }

    std::vector<AnalogKeyGroup> result;
    for (auto & [group_prefix, keys]: groups) {
        if (keys.size() >= 2) {
            std::sort(keys.begin(), keys.end());
            result.push_back({group_prefix, std::move(keys)});
        }
    }

    return result;
}

bool populateTensorFromAnalogKeys(
        DataManager & dm,
        std::string const & tensor_key,
        std::vector<std::string> const & analog_keys) {
    if (analog_keys.empty()) {
        spdlog::error("TensorColumnViews: no analog keys provided");
        return false;
    }

    auto existing_tensor = dm.getData<TensorData>(tensor_key);
    if (!existing_tensor) {
        spdlog::error(
                "TensorColumnViews: no TensorData at '{}'", tensor_key);
        return false;
    }

    auto const tensor_time_key = dm.getTimeKey(tensor_key);
    auto const analog_time_key = dm.getTimeKey(analog_keys.front());
    if (!tensor_time_key.empty() && tensor_time_key != analog_time_key) {
        spdlog::error(
                "TensorColumnViews: TimeFrame mismatch — tensor '{}' uses "
                "TimeKey '{}' but analog channels use '{}'",
                tensor_key,
                tensor_time_key.str(),
                analog_time_key.str());
        return false;
    }

    std::vector<std::shared_ptr<AnalogTimeSeries const>> channels;
    channels.reserve(analog_keys.size());

    for (auto const & key: analog_keys) {
        auto analog = dm.getData<AnalogTimeSeries>(key);
        if (!analog) {
            spdlog::error(
                    "TensorColumnViews: no AnalogTimeSeries at '{}'", key);
            return false;
        }
        channels.push_back(std::move(analog));
    }

    using namespace Neuralyzer::Transforms::V2;
    using namespace Neuralyzer::Transforms::V2::Examples;

    AnalogToTensorParams params;
    params.channel_keys = analog_keys;

    ComputeContext const ctx;
    auto new_tensor = analogToTensor(channels, params, ctx);
    if (!new_tensor) {
        return false;
    }

    dm.setData<TensorData>(tensor_key, new_tensor, analog_time_key);
    return true;
}

}// namespace Neuralyzer::TensorDesign
