/**
 * @file TensorToAnalog.cpp
 * @brief Implementation of TensorToAnalog container transform
 */

#include "TensorToAnalog.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

auto tensorToAnalog(
        TensorData const & input,
        TensorToAnalogParams const & params,
        ComputeContext const & ctx) -> std::vector<std::shared_ptr<AnalogTimeSeries>> {

    if (input.ndim() != 2) {
        ctx.logMessage("TensorToAnalog: input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return {};
    }

    auto const num_cols = input.numColumns();

    // Determine which columns to extract
    std::vector<int> columns = params.columns;
    if (columns.empty()) {
        columns.resize(num_cols);
        std::iota(columns.begin(), columns.end(), 0);
    }

    // Validate column indices
    for (auto col_idx: columns) {
        if (col_idx < 0 || static_cast<std::size_t>(col_idx) >= num_cols) {
            ctx.logMessage("TensorToAnalog: column index " + std::to_string(col_idx) +
                           " out of range [0, " + std::to_string(num_cols) + ")");
            return {};
        }
    }

    ctx.reportProgress(0);

    if (ctx.shouldCancel()) {
        return {};
    }

    // Extract time information from TensorData
    auto const & row_desc = input.rows();
    auto const row_type = row_desc.type();
    auto time_frame = input.getTimeFrame();

    std::vector<std::shared_ptr<AnalogTimeSeries>> result;
    result.reserve(columns.size());

    for (int column : columns) {
        auto const col_idx = static_cast<std::size_t>(column);
        auto values = input.getColumn(col_idx);

        std::shared_ptr<AnalogTimeSeries> analog;

        if (row_type == RowType::TimeFrameIndex) {
            // Preserve time indices from the tensor
            auto time_indices = row_desc.timeStoragePtr()->getAllTimeIndices();
            analog = std::make_shared<AnalogTimeSeries>(std::move(values), std::move(time_indices));
            if (time_frame) {
                analog->setTimeFrame(time_frame);
            }
        } else {
            // Ordinal or Interval: use consecutive indices
            auto const n = values.size();
            analog = std::make_shared<AnalogTimeSeries>(std::move(values), n);
            if (time_frame) {
                analog->setTimeFrame(time_frame);
            }
        }

        result.push_back(std::move(analog));

        if (ctx.shouldCancel()) {
            return {};
        }
    }

    ctx.reportProgress(100);
    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
