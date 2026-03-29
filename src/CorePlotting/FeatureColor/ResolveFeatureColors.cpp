/**
 * @file ResolveFeatureColors.cpp
 * @brief Implementation of per-point float resolution from external color data sources
 */

#include "ResolveFeatureColors.hpp"

#include "DataManager/DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_map>

namespace CorePlotting::FeatureColor {

namespace {

/**
 * @brief ATS join: look up each point's time in the AnalogTimeSeries
 */
std::vector<std::optional<float>> resolveFromATS(
        std::shared_ptr<AnalogTimeSeries> const & ats,
        std::span<TimeFrameIndex const> point_times) {

    std::vector<std::optional<float>> result(point_times.size());
    for (std::size_t i = 0; i < point_times.size(); ++i) {
        result[i] = ats->getAtTime(point_times[i]);
    }
    return result;
}

/**
 * @brief Extract the float column from TensorData by name or index
 */
std::vector<float> extractTensorColumn(
        TensorData const & tensor,
        FeatureColorSourceDescriptor const & descriptor) {

    if (descriptor.tensor_column_name.has_value()) {
        return tensor.getColumn(descriptor.tensor_column_name.value()); // NOLINT(bugprone-unchecked-optional-access)
    }
    assert(descriptor.tensor_column_index.has_value());
    return tensor.getColumn(descriptor.tensor_column_index.value()); // NOLINT(bugprone-unchecked-optional-access)
}

/**
 * @brief TensorData TFI join: build TFI→row map, then look up each point
 */
std::vector<std::optional<float>> resolveFromTensorTFI(
        std::shared_ptr<TensorData> const & tensor,
        FeatureColorSourceDescriptor const & descriptor,
        std::span<TimeFrameIndex const> point_times) {

    auto const & rows = tensor->rows();
    auto const & time_storage = rows.timeStorage();
    std::size_t const row_count = rows.count();

    auto const column_data = extractTensorColumn(*tensor, descriptor);

    // Build TFI → row index map for O(1) lookup per point
    std::unordered_map<int64_t, std::size_t> tfi_to_row;
    tfi_to_row.reserve(row_count);
    for (std::size_t r = 0; r < row_count; ++r) {
        auto const tfi = time_storage.getTimeFrameIndexAt(r);
        tfi_to_row[tfi.getValue()] = r;
    }

    std::vector<std::optional<float>> result(point_times.size());
    for (std::size_t i = 0; i < point_times.size(); ++i) {
        auto const it = tfi_to_row.find(point_times[i].getValue());
        if (it != tfi_to_row.end() && it->second < column_data.size()) {
            result[i] = column_data[it->second];
        }
    }
    return result;
}

/**
 * @brief TensorData ordinal join: row i maps to point i
 */
std::vector<std::optional<float>> resolveFromTensorOrdinal(
        std::shared_ptr<TensorData> const & tensor,
        FeatureColorSourceDescriptor const & descriptor,
        std::span<TimeFrameIndex const> point_times) {

    auto const column_data = extractTensorColumn(*tensor, descriptor);

    std::vector<std::optional<float>> result(point_times.size());
    std::size_t const shared_count = std::min(point_times.size(), column_data.size());
    for (std::size_t i = 0; i < shared_count; ++i) {
        result[i] = column_data[i];
    }
    return result;
}

/**
 * @brief DIS containment join: 1.0 if point time is inside an interval, 0.0 otherwise
 */
std::vector<std::optional<float>> resolveFromDIS(
        std::shared_ptr<DigitalIntervalSeries> const & dis,
        std::span<TimeFrameIndex const> point_times,
        std::shared_ptr<TimeFrame> const & point_time_frame) {

    std::vector<std::optional<float>> result(point_times.size());

    if (!point_time_frame) {
        return result;
    }

    for (std::size_t i = 0; i < point_times.size(); ++i) {
        bool const inside = dis->hasIntervalAtTime(point_times[i], *point_time_frame);
        result[i] = inside ? 1.0f : 0.0f;
    }
    return result;
}

}// anonymous namespace

std::vector<std::optional<float>> resolveFeatureColors(
        DataManager & dm,
        FeatureColorSourceDescriptor const & descriptor,
        std::span<TimeFrameIndex const> point_times,
        std::shared_ptr<TimeFrame> const & point_time_frame) {

    assert(!descriptor.data_key.empty() && "resolveFeatureColors: data_key must not be empty");

    if (point_times.empty()) {
        return {};
    }

    // Try AnalogTimeSeries
    if (auto ats = dm.getData<AnalogTimeSeries>(descriptor.data_key)) {
        return resolveFromATS(ats, point_times);
    }

    // Try TensorData
    if (auto tensor = dm.getData<TensorData>(descriptor.data_key)) {
        auto const row_type = tensor->rowType();
        if (row_type == RowType::TimeFrameIndex) {
            return resolveFromTensorTFI(tensor, descriptor, point_times);
        }
        // Interval and Ordinal both use positional join
        return resolveFromTensorOrdinal(tensor, descriptor, point_times);
    }

    // Try DigitalIntervalSeries
    if (auto dis = dm.getData<DigitalIntervalSeries>(descriptor.data_key)) {
        return resolveFromDIS(dis, point_times, point_time_frame);
    }

    // Key not found — return all nullopt
    return std::vector<std::optional<float>>(point_times.size());
}

}// namespace CorePlotting::FeatureColor
