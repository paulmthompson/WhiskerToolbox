#include "AnalogTimestampOffsetsMultiComputer.h"

#include "utils/TableView/interfaces/IAnalogSource.h"
#include "utils/TableView/core/ExecutionPlan.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

auto AnalogTimestampOffsetsMultiComputer::computeBatch(ExecutionPlan const & plan) const -> std::vector<std::vector<double>> {
    // Determine base indices from plan: prefer entity-expanded rows, then timestamp indices, else intervals' starts
    std::vector<TimeFrameIndex> baseIndices;
    if (!plan.getRows().empty()) {
        auto const & rows = plan.getRows();
        baseIndices.reserve(rows.size());
        for (auto const & r : rows) {
            baseIndices.push_back(r.timeIndex);
        }
    } else if (plan.hasIndices()) {
        baseIndices = plan.getIndices();
    } else if (plan.hasIntervals()) {
        auto const & intervals = plan.getIntervals();
        baseIndices.reserve(intervals.size());
        for (auto const & itv : intervals) {
            baseIndices.push_back(itv.start);
        }
    } else {
        throw std::runtime_error("ExecutionPlan contains no indices or intervals");
    }

    auto planTimeFrame = plan.getTimeFrame();
    if (!planTimeFrame) {
        throw std::runtime_error("AnalogTimestampOffsetsMultiComputer requires a non-null TimeFrame");
    }

    size_t const rowCount = baseIndices.size();
    std::vector<std::vector<double>> outputs;
    outputs.resize(m_offsets.size());
    for (auto & vec : outputs) vec.resize(rowCount);

    // For each offset, compute the shifted indices and fetch values
    for (size_t oi = 0; oi < m_offsets.size(); ++oi) {
        int const delta = m_offsets[oi];
        for (size_t r = 0; r < rowCount; ++r) {
            TimeFrameIndex shifted(baseIndices[r].getValue() + static_cast<int64_t>(delta));
            // Delegate timeframe conversion to the adapter using the plan's timeframe
            auto slice = m_source->getDataInRange(shifted, shifted, planTimeFrame.get());
            if (slice.empty()) {
                outputs[oi][r] = std::numeric_limits<double>::quiet_NaN();
            } else {
                outputs[oi][r] = static_cast<double>(slice.front());
            }
        }
    }

    return outputs;
}

auto AnalogTimestampOffsetsMultiComputer::getOutputNames() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(m_offsets.size());
    for (int off : m_offsets) {
        if (off == 0) {
            names.emplace_back(".t+0");
        } else if (off > 0) {
            names.emplace_back(".t+" + std::to_string(off));
        } else {
            names.emplace_back(".t" + std::to_string(off)); // off is negative, already has '-'
        }
    }
    return names;
}


