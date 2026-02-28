#include "SourceCompatibility.hpp"

#include "ScatterAxisSource.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"

ScatterSourceRowType resolveSourceRowType(
    DataManager & dm,
    ScatterAxisSource const & source)
{
    if (source.data_key.empty()) {
        return ScatterSourceRowType::Unknown;
    }

    // Check AnalogTimeSeries first
    auto analog = dm.getData<AnalogTimeSeries>(source.data_key);
    if (analog) {
        return ScatterSourceRowType::AnalogTimeSeries;
    }

    // Check TensorData
    auto tensor = dm.getData<TensorData>(source.data_key);
    if (tensor) {
        switch (tensor->rowType()) {
            case RowType::TimeFrameIndex:
                return ScatterSourceRowType::TensorTimeFrameIndex;
            case RowType::Interval:
                return ScatterSourceRowType::TensorInterval;
            case RowType::Ordinal:
                return ScatterSourceRowType::TensorOrdinal;
        }
    }

    return ScatterSourceRowType::Unknown;
}

namespace {

/**
 * @brief Get the row count for an ordinal TensorData source
 *
 * Returns std::nullopt if the key is not a TensorData.
 */
std::optional<std::size_t> getOrdinalRowCount(
    DataManager & dm,
    ScatterAxisSource const & source)
{
    auto tensor = dm.getData<TensorData>(source.data_key);
    if (tensor && tensor->rowType() == RowType::Ordinal) {
        return tensor->numRows();
    }
    return std::nullopt;
}

/**
 * @brief Convert a ScatterSourceRowType to a human-readable label
 */
std::string rowTypeLabel(ScatterSourceRowType type)
{
    switch (type) {
        case ScatterSourceRowType::AnalogTimeSeries:     return "AnalogTimeSeries";
        case ScatterSourceRowType::TensorTimeFrameIndex: return "TensorData (TimeFrameIndex rows)";
        case ScatterSourceRowType::TensorInterval:       return "TensorData (Interval rows)";
        case ScatterSourceRowType::TensorOrdinal:        return "TensorData (Ordinal rows)";
        case ScatterSourceRowType::Unknown:              return "Unknown";
    }
    return "Unknown";
}

}  // namespace

SourceCompatibilityResult checkSourceCompatibility(
    DataManager & dm,
    ScatterAxisSource const & x_source,
    ScatterAxisSource const & y_source)
{
    SourceCompatibilityResult result;
    result.x_row_type = resolveSourceRowType(dm, x_source);
    result.y_row_type = resolveSourceRowType(dm, y_source);

    // If either source could not be resolved, mark incompatible
    if (result.x_row_type == ScatterSourceRowType::Unknown) {
        result.compatible = false;
        result.warning_message = "X-axis data source \"" + x_source.data_key
            + "\" not found or is not an AnalogTimeSeries/TensorData.";
        return result;
    }
    if (result.y_row_type == ScatterSourceRowType::Unknown) {
        result.compatible = false;
        result.warning_message = "Y-axis data source \"" + y_source.data_key
            + "\" not found or is not an AnalogTimeSeries/TensorData.";
        return result;
    }

    // ----- Compatibility matrix -----
    //
    // AnalogTimeSeries is always TimeFrameIndex-based, so it is compatible
    // with other TimeFrameIndex sources (ATS or TFI TensorData).

    auto const x = result.x_row_type;
    auto const y = result.y_row_type;

    auto isTimeFrameIndexLike = [](ScatterSourceRowType t) {
        return t == ScatterSourceRowType::AnalogTimeSeries
            || t == ScatterSourceRowType::TensorTimeFrameIndex;
    };

    // Case 1: Both are TimeFrameIndex-like (ATS or TFI TensorData)
    if (isTimeFrameIndexLike(x) && isTimeFrameIndexLike(y)) {
        result.compatible = true;
        return result;
    }

    // Case 2: Both are Interval TensorData
    if (x == ScatterSourceRowType::TensorInterval
        && y == ScatterSourceRowType::TensorInterval) {
        result.compatible = true;
        return result;
    }

    // Case 3: Both are Ordinal TensorData — require same row count
    if (x == ScatterSourceRowType::TensorOrdinal
        && y == ScatterSourceRowType::TensorOrdinal) {
        auto x_count = getOrdinalRowCount(dm, x_source);
        auto y_count = getOrdinalRowCount(dm, y_source);
        if (x_count.has_value() && y_count.has_value() && *x_count == *y_count) {
            result.compatible = true;
        } else {
            result.compatible = false;
            result.warning_message = "Ordinal TensorData sources have different row counts ("
                + (x_count ? std::to_string(*x_count) : "?") + " vs "
                + (y_count ? std::to_string(*y_count) : "?") + ").";
        }
        return result;
    }

    // All other combinations are incompatible
    result.compatible = false;
    result.warning_message = "Incompatible row types: X is "
        + rowTypeLabel(x) + ", Y is " + rowTypeLabel(y) + ".";
    return result;
}
