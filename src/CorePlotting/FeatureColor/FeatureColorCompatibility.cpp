/**
 * @file FeatureColorCompatibility.cpp
 * @brief Implementation of feature color source compatibility checking
 */

#include "FeatureColorCompatibility.hpp"

#include "DataManager/DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/TensorData.hpp"

namespace CorePlotting::FeatureColor {

namespace {

FeatureColorCompatibilityResult checkATS(
        DataManager & dm,
        FeatureColorSourceDescriptor const & descriptor) {
    auto ats = dm.getData<AnalogTimeSeries>(descriptor.data_key);
    if (!ats) {
        return {false, "Key '" + descriptor.data_key + "' is not an AnalogTimeSeries"};
    }
    return {true, {}};
}

FeatureColorCompatibilityResult checkTensor(
        DataManager & dm,
        FeatureColorSourceDescriptor const & descriptor) {
    auto tensor = dm.getData<TensorData>(descriptor.data_key);
    if (!tensor) {
        return {false, "Key '" + descriptor.data_key + "' is not a TensorData"};
    }

    // Validate column specification
    if (descriptor.tensor_column_name.has_value()) {
        auto const & names = tensor->columnNames();
        auto const & col_name = *descriptor.tensor_column_name;
        if (std::find(names.begin(), names.end(), col_name) == names.end()) {
            return {false, "TensorData '" + descriptor.data_key +
                                   "' does not have column '" + col_name + "'"};
        }
    } else if (descriptor.tensor_column_index.has_value()) {
        if (*descriptor.tensor_column_index >= tensor->numColumns()) {
            return {false, "TensorData '" + descriptor.data_key +
                                   "' column index " +
                                   std::to_string(*descriptor.tensor_column_index) +
                                   " out of range (has " +
                                   std::to_string(tensor->numColumns()) + " columns)"};
        }
    } else {
        return {false, "TensorData source requires tensor_column_name or tensor_column_index"};
    }

    return {true, {}};
}

FeatureColorCompatibilityResult checkDIS(
        DataManager & dm,
        FeatureColorSourceDescriptor const & descriptor) {
    auto dis = dm.getData<DigitalIntervalSeries>(descriptor.data_key);
    if (!dis) {
        return {false, "Key '" + descriptor.data_key + "' is not a DigitalIntervalSeries"};
    }
    return {true, {}};
}

}// anonymous namespace

FeatureColorCompatibilityResult checkFeatureColorCompatibility(
        DataManager & dm,
        FeatureColorSourceDescriptor const & descriptor,
        PlotSourceRowType /*source_type*/,
        std::size_t /*point_count*/) {

    if (descriptor.data_key.empty()) {
        return {false, "No data key specified"};
    }

    // Try each supported type in order: ATS, TensorData, DIS
    if (dm.getData<AnalogTimeSeries>(descriptor.data_key)) {
        return checkATS(dm, descriptor);
    }
    if (dm.getData<TensorData>(descriptor.data_key)) {
        return checkTensor(dm, descriptor);
    }
    if (dm.getData<DigitalIntervalSeries>(descriptor.data_key)) {
        return checkDIS(dm, descriptor);
    }

    return {false, "Key '" + descriptor.data_key + "' not found or is an unsupported type"};
}

}// namespace CorePlotting::FeatureColor
