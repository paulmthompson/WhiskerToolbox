#include "LineDataAdapter.h"

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