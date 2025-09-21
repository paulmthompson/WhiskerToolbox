#include "LineDataAdapter.h"

#include "Lines/Line_Data.hpp"

#include <stdexcept>

LineDataAdapter::LineDataAdapter(std::shared_ptr<LineData> lineData,
                                 std::shared_ptr<TimeFrame> timeFrame,
                                 std::string name)
    : m_lineData(std::move(lineData)),
      m_timeFrame(std::move(timeFrame)),
      m_name(std::move(name)) {
    if (!m_lineData) {
        throw std::invalid_argument("LineData cannot be null");
    }
}

auto LineDataAdapter::getName() const -> std::string const & {
    return m_name;
}

auto LineDataAdapter::getTimeFrame() const -> std::shared_ptr<TimeFrame> {
    return m_timeFrame;
}

auto LineDataAdapter::size() const -> size_t {
    // Count total number of lines across all time frames
    size_t totalLines = 0;
    for (auto const & [time, lines]: m_lineData->GetAllLinesAsRange()) {
        totalLines += lines.size();
    }
    return totalLines;
}

auto LineDataAdapter::getLines() -> std::vector<Line2D> {
    std::vector<Line2D> allLines;

    // Collect all lines from all time frames
    for (auto const & [time, lines]: m_lineData->GetAllLinesAsRange()) {
        allLines.insert(allLines.end(), lines.begin(), lines.end());
    }

    return allLines;
}

auto LineDataAdapter::getLinesInRange(TimeFrameIndex start,
                                      TimeFrameIndex end,
                                      TimeFrame const * target_timeFrame) -> std::vector<Line2D> {
    // Fast path: when start == end, avoid constructing a ranges pipeline.
    // Directly fetch the lines at the single time index using timeframe conversion.
    if (start == end) {
        auto const & lines_ref = m_lineData->getAtTime(start, target_timeFrame, m_timeFrame.get());
        return {lines_ref.begin(), lines_ref.end()};
    }

    // Use the LineData's built-in method to get lines in the time range.
    // This method handles the time frame conversion internally via a lazy view.
    auto lines_view = m_lineData->GetLinesInRange(TimeFrameInterval(start, end),
                                                  target_timeFrame,
                                                  m_timeFrame.get());

    // Materialize the view into a vector
    std::vector<Line2D> result;
    for (auto const & timeLinesPair: lines_view) {
        result.insert(result.end(), timeLinesPair.lines.begin(), timeLinesPair.lines.end());
    }

    return result;
}

bool LineDataAdapter::hasMultiSamples() const {
    // Check if any timestamp has more than one line
    for (auto const & [time, lines]: m_lineData->GetAllLinesAsRange()) {
        if (lines.size() > 1) {
            return true;
        }
    }
    return false;
}

auto LineDataAdapter::getEntityCountAt(TimeFrameIndex t) const -> size_t {
    auto const & lines_ref = m_lineData->getAtTime(t, m_timeFrame.get(), m_timeFrame.get());
    return lines_ref.size();
}

auto LineDataAdapter::getLineAt(TimeFrameIndex t, int entityIndex) const -> Line2D const * {
    auto const & lines_ref = m_lineData->getAtTime(t, m_timeFrame.get(), m_timeFrame.get());
    if (entityIndex < 0 || static_cast<size_t>(entityIndex) >= lines_ref.size()) {
        return nullptr;
    }
    return &lines_ref[static_cast<size_t>(entityIndex)];
}

EntityId LineDataAdapter::getEntityIdAt(TimeFrameIndex t, int entityIndex) const {
    auto const & ids = m_lineData->getEntityIdsAtTime(t);
    if (entityIndex < 0 || static_cast<size_t>(entityIndex) >= ids.size()) return 0;
    return ids[static_cast<size_t>(entityIndex)];
}

std::vector<EntityId> LineDataAdapter::getEntityIdsAtTime(TimeFrameIndex t,
                                                          TimeFrame const * target_timeframe) const {
    return m_lineData->getEntityIdsAtTime(t, target_timeframe, m_timeFrame.get());
}