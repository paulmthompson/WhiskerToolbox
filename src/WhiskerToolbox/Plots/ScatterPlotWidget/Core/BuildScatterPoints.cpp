#include "BuildScatterPoints.hpp"

#include "ScatterAxisSource.hpp"
#include "SourceCompatibility.hpp"
#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_map>

namespace {

/**
 * @brief Extract column values from a TensorData source
 *
 * Uses tensor_column_name first, then tensor_column_index as fallback.
 */
std::vector<float> extractTensorColumn(
    TensorData const & tensor,
    ScatterAxisSource const & source)
{
    if (source.tensor_column_name.has_value() && !source.tensor_column_name->empty()) {
        return tensor.getColumn(*source.tensor_column_name);
    }
    if (source.tensor_column_index.has_value()) {
        return tensor.getColumn(*source.tensor_column_index);
    }
    // Default to first column
    return tensor.getColumn(static_cast<std::size_t>(0));
}

/**
 * @brief Get values from an AnalogTimeSeries as a simple span
 *
 * Returns a parallel pair of (time_indices, values) for the full series.
 */
struct AnalogData {
    std::vector<TimeFrameIndex> time_indices;
    std::span<float const> values;
    std::shared_ptr<TimeIndexStorage> time_storage;
};

AnalogData getAnalogData(AnalogTimeSeries const & ats)
{
    AnalogData result;
    result.time_storage = ats.getTimeStorage();
    result.time_indices = result.time_storage->getAllTimeIndices();
    result.values = ats.getAnalogTimeSeries();
    return result;
}

/**
 * @brief Build scatter points from two AnalogTimeSeries sources
 *
 * Iterates over the intersection of valid TimeFrameIndex values.
 * Temporal offsets shift the lookup index.
 */
ScatterPointData buildFromAnalogAnalog(
    AnalogTimeSeries const & x_ats,
    AnalogTimeSeries const & y_ats,
    int x_offset,
    int y_offset)
{
    ScatterPointData result;

    auto x_data = getAnalogData(x_ats);
    auto y_data = getAnalogData(y_ats);

    if (x_data.values.empty() || y_data.values.empty()) {
        return result;
    }

    // Build a lookup from TimeFrameIndex -> array position for Y
    std::unordered_map<int64_t, std::size_t> y_index_map;
    y_index_map.reserve(y_data.time_indices.size());
    for (std::size_t i = 0; i < y_data.time_indices.size(); ++i) {
        y_index_map[y_data.time_indices[i].getValue()] = i;
    }

    // Iterate over X time indices, looking up corresponding Y values
    for (std::size_t xi = 0; xi < x_data.time_indices.size(); ++xi) {
        auto const base_tfi = x_data.time_indices[xi];
        auto const x_lookup = TimeFrameIndex(base_tfi.getValue() + x_offset);
        auto const y_lookup = TimeFrameIndex(base_tfi.getValue() + y_offset);

        // Check X is within bounds after offset
        auto x_pos = x_data.time_storage->findArrayPositionForTimeIndex(x_lookup);
        if (!x_pos.has_value()) {
            continue;
        }

        // Check Y exists at the offset index
        auto it = y_index_map.find(y_lookup.getValue());
        if (it == y_index_map.end()) {
            continue;
        }

        result.x_values.push_back(x_data.values[*x_pos]);
        result.y_values.push_back(y_data.values[it->second]);
        result.time_indices.push_back(base_tfi);
    }

    return result;
}

/**
 * @brief Build scatter points from AnalogTimeSeries (X) and TensorData with TFI rows (Y)
 */
ScatterPointData buildFromAnalogTensorTFI(
    AnalogTimeSeries const & x_ats,
    TensorData const & y_tensor,
    ScatterAxisSource const & y_source,
    int x_offset,
    int y_offset)
{
    ScatterPointData result;

    auto x_data = getAnalogData(x_ats);
    auto y_col = extractTensorColumn(y_tensor, y_source);
    auto const & y_time_storage = y_tensor.rows().timeStorage();
    auto y_time_indices = y_time_storage.getAllTimeIndices();

    if (x_data.values.empty() || y_col.empty()) {
        return result;
    }

    // Build lookup for Y tensor time indices
    std::unordered_map<int64_t, std::size_t> y_index_map;
    y_index_map.reserve(y_time_indices.size());
    for (std::size_t i = 0; i < y_time_indices.size(); ++i) {
        y_index_map[y_time_indices[i].getValue()] = i;
    }

    for (std::size_t xi = 0; xi < x_data.time_indices.size(); ++xi) {
        auto const base_tfi = x_data.time_indices[xi];
        auto const x_lookup = TimeFrameIndex(base_tfi.getValue() + x_offset);
        auto const y_lookup = TimeFrameIndex(base_tfi.getValue() + y_offset);

        auto x_pos = x_data.time_storage->findArrayPositionForTimeIndex(x_lookup);
        if (!x_pos.has_value()) {
            continue;
        }

        auto it = y_index_map.find(y_lookup.getValue());
        if (it == y_index_map.end()) {
            continue;
        }

        result.x_values.push_back(x_data.values[*x_pos]);
        result.y_values.push_back(y_col[it->second]);
        result.time_indices.push_back(base_tfi);
    }

    return result;
}

/**
 * @brief Build scatter points from TensorData (X, TFI) and AnalogTimeSeries (Y)
 */
ScatterPointData buildFromTensorTFIAnalog(
    TensorData const & x_tensor,
    ScatterAxisSource const & x_source,
    AnalogTimeSeries const & y_ats,
    int x_offset,
    int y_offset)
{
    ScatterPointData result;

    auto x_col = extractTensorColumn(x_tensor, x_source);
    auto const & x_time_storage = x_tensor.rows().timeStorage();
    auto x_time_indices = x_time_storage.getAllTimeIndices();
    auto y_data = getAnalogData(y_ats);

    if (x_col.empty() || y_data.values.empty()) {
        return result;
    }

    // Build lookup for Y analog time indices
    std::unordered_map<int64_t, std::size_t> y_index_map;
    y_index_map.reserve(y_data.time_indices.size());
    for (std::size_t i = 0; i < y_data.time_indices.size(); ++i) {
        y_index_map[y_data.time_indices[i].getValue()] = i;
    }

    for (std::size_t xi = 0; xi < x_time_indices.size(); ++xi) {
        auto const base_tfi = x_time_indices[xi];
        auto const x_lookup = TimeFrameIndex(base_tfi.getValue() + x_offset);
        auto const y_lookup = TimeFrameIndex(base_tfi.getValue() + y_offset);

        auto x_pos = x_time_storage.findArrayPositionForTimeIndex(x_lookup);
        if (!x_pos.has_value()) {
            continue;
        }

        auto it = y_index_map.find(y_lookup.getValue());
        if (it == y_index_map.end()) {
            continue;
        }

        result.x_values.push_back(x_col[*x_pos]);
        result.y_values.push_back(y_data.values[it->second]);
        result.time_indices.push_back(base_tfi);
    }

    return result;
}

/**
 * @brief Build scatter points from two TensorData sources with TFI rows
 */
ScatterPointData buildFromTensorTFITensorTFI(
    TensorData const & x_tensor,
    ScatterAxisSource const & x_source,
    TensorData const & y_tensor,
    ScatterAxisSource const & y_source,
    int x_offset,
    int y_offset)
{
    ScatterPointData result;

    auto x_col = extractTensorColumn(x_tensor, x_source);
    auto y_col = extractTensorColumn(y_tensor, y_source);
    auto const & x_time_storage = x_tensor.rows().timeStorage();
    auto x_time_indices = x_time_storage.getAllTimeIndices();
    auto const & y_time_storage = y_tensor.rows().timeStorage();
    auto y_time_indices = y_time_storage.getAllTimeIndices();

    if (x_col.empty() || y_col.empty()) {
        return result;
    }

    // Build lookup for Y
    std::unordered_map<int64_t, std::size_t> y_index_map;
    y_index_map.reserve(y_time_indices.size());
    for (std::size_t i = 0; i < y_time_indices.size(); ++i) {
        y_index_map[y_time_indices[i].getValue()] = i;
    }

    for (std::size_t xi = 0; xi < x_time_indices.size(); ++xi) {
        auto const base_tfi = x_time_indices[xi];
        auto const x_lookup = TimeFrameIndex(base_tfi.getValue() + x_offset);
        auto const y_lookup = TimeFrameIndex(base_tfi.getValue() + y_offset);

        auto x_pos = x_time_storage.findArrayPositionForTimeIndex(x_lookup);
        if (!x_pos.has_value()) {
            continue;
        }

        auto it = y_index_map.find(y_lookup.getValue());
        if (it == y_index_map.end()) {
            continue;
        }

        result.x_values.push_back(x_col[*x_pos]);
        result.y_values.push_back(y_col[it->second]);
        result.time_indices.push_back(base_tfi);
    }

    return result;
}

/**
 * @brief Build scatter points from two Ordinal TensorData sources (positional pairing)
 */
ScatterPointData buildFromOrdinalOrdinal(
    TensorData const & x_tensor,
    ScatterAxisSource const & x_source,
    TensorData const & y_tensor,
    ScatterAxisSource const & y_source)
{
    ScatterPointData result;

    auto x_col = extractTensorColumn(x_tensor, x_source);
    auto y_col = extractTensorColumn(y_tensor, y_source);

    auto const count = std::min(x_col.size(), y_col.size());
    result.x_values.reserve(count);
    result.y_values.reserve(count);
    result.time_indices.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        result.x_values.push_back(x_col[i]);
        result.y_values.push_back(y_col[i]);
        result.time_indices.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }

    return result;
}

/**
 * @brief Build scatter points from two Interval TensorData sources (positional pairing)
 */
ScatterPointData buildFromIntervalInterval(
    TensorData const & x_tensor,
    ScatterAxisSource const & x_source,
    TensorData const & y_tensor,
    ScatterAxisSource const & y_source)
{
    ScatterPointData result;

    auto x_col = extractTensorColumn(x_tensor, x_source);
    auto y_col = extractTensorColumn(y_tensor, y_source);

    auto const count = std::min(x_col.size(), y_col.size());
    result.x_values.reserve(count);
    result.y_values.reserve(count);
    result.time_indices.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        result.x_values.push_back(x_col[i]);
        result.y_values.push_back(y_col[i]);
        // Use the start of the interval as the TimeFrameIndex
        auto intervals = x_tensor.rows().intervals();
        if (i < intervals.size()) {
            result.time_indices.push_back(TimeFrameIndex(
                intervals[i].start.getValue()));
        } else {
            result.time_indices.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
        }
    }

    return result;
}

}  // namespace

ScatterPointData buildScatterPoints(
    DataManager & dm,
    ScatterAxisSource const & x_source,
    ScatterAxisSource const & y_source)
{
    // Resolve source types
    auto const x_type = resolveSourceRowType(dm, x_source);
    auto const y_type = resolveSourceRowType(dm, y_source);

    int const x_offset = x_source.time_offset;
    int const y_offset = y_source.time_offset;

    // AnalogTimeSeries x AnalogTimeSeries
    if (x_type == ScatterSourceRowType::AnalogTimeSeries
        && y_type == ScatterSourceRowType::AnalogTimeSeries) {
        auto x_ats = dm.getData<AnalogTimeSeries>(x_source.data_key);
        auto y_ats = dm.getData<AnalogTimeSeries>(y_source.data_key);
        if (x_ats && y_ats) {
            return buildFromAnalogAnalog(*x_ats, *y_ats, x_offset, y_offset);
        }
    }

    // AnalogTimeSeries x TensorData (TFI)
    if (x_type == ScatterSourceRowType::AnalogTimeSeries
        && y_type == ScatterSourceRowType::TensorTimeFrameIndex) {
        auto x_ats = dm.getData<AnalogTimeSeries>(x_source.data_key);
        auto y_tensor = dm.getData<TensorData>(y_source.data_key);
        if (x_ats && y_tensor) {
            return buildFromAnalogTensorTFI(*x_ats, *y_tensor, y_source, x_offset, y_offset);
        }
    }

    // TensorData (TFI) x AnalogTimeSeries
    if (x_type == ScatterSourceRowType::TensorTimeFrameIndex
        && y_type == ScatterSourceRowType::AnalogTimeSeries) {
        auto x_tensor = dm.getData<TensorData>(x_source.data_key);
        auto y_ats = dm.getData<AnalogTimeSeries>(y_source.data_key);
        if (x_tensor && y_ats) {
            return buildFromTensorTFIAnalog(*x_tensor, x_source, *y_ats, x_offset, y_offset);
        }
    }

    // TensorData (TFI) x TensorData (TFI)
    if (x_type == ScatterSourceRowType::TensorTimeFrameIndex
        && y_type == ScatterSourceRowType::TensorTimeFrameIndex) {
        auto x_tensor = dm.getData<TensorData>(x_source.data_key);
        auto y_tensor = dm.getData<TensorData>(y_source.data_key);
        if (x_tensor && y_tensor) {
            return buildFromTensorTFITensorTFI(*x_tensor, x_source, *y_tensor, y_source,
                                               x_offset, y_offset);
        }
    }

    // TensorData (Ordinal) x TensorData (Ordinal)
    if (x_type == ScatterSourceRowType::TensorOrdinal
        && y_type == ScatterSourceRowType::TensorOrdinal) {
        auto x_tensor = dm.getData<TensorData>(x_source.data_key);
        auto y_tensor = dm.getData<TensorData>(y_source.data_key);
        if (x_tensor && y_tensor) {
            return buildFromOrdinalOrdinal(*x_tensor, x_source, *y_tensor, y_source);
        }
    }

    // TensorData (Interval) x TensorData (Interval)
    if (x_type == ScatterSourceRowType::TensorInterval
        && y_type == ScatterSourceRowType::TensorInterval) {
        auto x_tensor = dm.getData<TensorData>(x_source.data_key);
        auto y_tensor = dm.getData<TensorData>(y_source.data_key);
        if (x_tensor && y_tensor) {
            return buildFromIntervalInterval(*x_tensor, x_source, *y_tensor, y_source);
        }
    }

    // Incompatible or unknown — return empty
    return {};
}
