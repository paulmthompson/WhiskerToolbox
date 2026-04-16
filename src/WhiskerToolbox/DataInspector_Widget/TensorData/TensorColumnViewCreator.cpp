/**
 * @file TensorColumnViewCreator.cpp
 * @brief Implementation of TensorData ↔ AnalogTimeSeries bridge utilities
 */

#include "TensorColumnViewCreator.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include "TransformsV2/algorithms/AnalogToTensor/AnalogToTensor.hpp"
#include "TransformsV2/algorithms/TensorToAnalog/TensorToAnalog.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <numeric>
#include <string>

auto createTensorColumnViews(
        DataManager & dm,
        std::string const & tensor_key,
        std::string const & prefix,
        std::vector<int> const & columns) -> std::size_t {

    auto tensor = dm.getData<TensorData>(tensor_key);
    if (!tensor) {
        std::cerr << "TensorColumnViewCreator: no TensorData at key '" << tensor_key << "'\n";
        return 0;
    }

    if (tensor->ndim() != 2) {
        std::cerr << "TensorColumnViewCreator: TensorData at '" << tensor_key
                  << "' is " << tensor->ndim() << "D, expected 2D\n";
        return 0;
    }

    using namespace WhiskerToolbox::Transforms::V2;
    using namespace WhiskerToolbox::Transforms::V2::Examples;

    TensorToAnalogParams params;
    params.columns = columns;

    ComputeContext const ctx;
    auto views = tensorToAnalog(*tensor, params, ctx);

    if (views.empty()) {
        return 0;
    }

    auto const & col_names = tensor->columnNames();
    auto const time_key = dm.getTimeKey(tensor_key);

    // Determine which columns were extracted (mirrors tensorToAnalog logic)
    std::vector<int> actual_columns = columns;
    if (actual_columns.empty()) {
        actual_columns.resize(tensor->numColumns());
        std::iota(actual_columns.begin(), actual_columns.end(), 0);
    }

    std::size_t count = 0;
    for (std::size_t i = 0; i < views.size(); ++i) {
        auto const col_idx = static_cast<std::size_t>(actual_columns[i]);

        // Use column name if available, otherwise fall back to index
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

auto discoverAnalogKeyGroups(DataManager & dm) -> std::vector<AnalogKeyGroup> {
    auto const analog_keys = dm.getKeys<AnalogTimeSeries>();

    std::map<std::string, std::vector<std::string>> groups;

    for (auto const & key: analog_keys) {
        auto const pos = key.rfind('_');
        if (pos == std::string::npos || pos == 0) {
            continue;// No prefix extractable
        }
        auto const prefix = key.substr(0, pos);
        groups[prefix].push_back(key);
    }

    std::vector<AnalogKeyGroup> result;
    for (auto & [prefix, keys]: groups) {
        if (keys.size() >= 2) {
            std::sort(keys.begin(), keys.end());
            result.push_back({prefix, std::move(keys)});
        }
    }

    return result;
}

auto populateTensorFromAnalogKeys(
        DataManager & dm,
        std::string const & tensor_key,
        std::vector<std::string> const & analog_keys) -> bool {

    if (analog_keys.empty()) {
        std::cerr << "populateTensorFromAnalogKeys: no analog keys provided\n";
        return false;
    }

    std::vector<std::shared_ptr<AnalogTimeSeries const>> channels;
    channels.reserve(analog_keys.size());

    for (auto const & key: analog_keys) {
        auto analog = dm.getData<AnalogTimeSeries>(key);
        if (!analog) {
            std::cerr << "populateTensorFromAnalogKeys: no AnalogTimeSeries at '"
                      << key << "'\n";
            return false;
        }
        channels.push_back(std::move(analog));
    }

    using namespace WhiskerToolbox::Transforms::V2;
    using namespace WhiskerToolbox::Transforms::V2::Examples;

    AnalogToTensorParams params;
    params.channel_keys = analog_keys;

    ComputeContext const ctx;
    auto new_tensor = analogToTensor(channels, params, ctx);

    if (!new_tensor) {
        return false;
    }

    auto const time_key = dm.getTimeKey(analog_keys.front());
    dm.setData<TensorData>(tensor_key, new_tensor, time_key);

    return true;
}
