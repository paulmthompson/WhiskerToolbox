/**
 * @file LineBatchData.cpp
 * @brief Implementation of LineBatchData queries and mutators
 */
#include "LineBatchData.hpp"

namespace CorePlotting {

std::uint32_t LineBatchData::numSegments() const
{
    return static_cast<std::uint32_t>(line_ids.size());
}

std::uint32_t LineBatchData::numLines() const
{
    return static_cast<std::uint32_t>(lines.size());
}

bool LineBatchData::empty() const
{
    return lines.empty();
}

void LineBatchData::clear()
{
    segments.clear();
    line_ids.clear();
    lines.clear();
    visibility_mask.clear();
    selection_mask.clear();
    canvas_width = 1.0f;
    canvas_height = 1.0f;
}

} // namespace CorePlotting
