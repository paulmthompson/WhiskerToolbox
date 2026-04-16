/**
 * @file TensorToAnalog.cpp
 * @brief Implementation of TensorToAnalog container transform
 *
 * Phase 3 update: uses zero-copy TensorColumnAnalogStorage views
 * instead of copying column data.
 */

#include "TensorToAnalog.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/storage/TensorColumnAnalogStorage.hpp"
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

    // Create a shared_ptr<TensorData const> for the storage views to keep alive.
    // We take the address of the input reference; the caller must ensure the
    // TensorData outlives the returned AnalogTimeSeries views. To guarantee
    // this, we create a shared_ptr with a no-op deleter (the caller owns the
    // TensorData) OR the caller can pass a shared_ptr externally.
    // For safety, copy the tensor into a shared_ptr so views are self-contained.
    auto tensor_ptr = std::make_shared<TensorData>(input);

    // Resolve time storage
    std::shared_ptr<TimeIndexStorage> time_storage;
    if (row_type == RowType::TimeFrameIndex) {
        time_storage = row_desc.timeStoragePtr();
    } else {
        time_storage = TimeIndexStorageFactory::createDenseFromZero(input.numRows());
    }

    std::vector<std::shared_ptr<AnalogTimeSeries>> result;
    result.reserve(columns.size());

    for (int const column: columns) {
        auto const col_idx = static_cast<std::size_t>(column);

        // Create zero-copy storage view into the tensor column
        auto col_storage = TensorColumnAnalogStorage(tensor_ptr, col_idx);
        AnalogDataStorageWrapper wrapper(col_storage);

        auto analog = AnalogTimeSeries::createFromStorage(
                std::move(wrapper), time_storage);

        if (time_frame) {
            analog->setTimeFrame(time_frame);
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
