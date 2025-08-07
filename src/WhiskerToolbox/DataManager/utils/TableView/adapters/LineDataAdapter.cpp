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

std::string const & LineDataAdapter::getName() const {
    return m_name;
}

std::shared_ptr<TimeFrame> LineDataAdapter::getTimeFrame() const {
    return m_timeFrame;
}

size_t LineDataAdapter::size() const {
    // Count total number of lines across all time frames
    size_t totalLines = 0;
    for (auto const & [time, lines]: m_lineData->GetAllLinesAsRange()) {
        totalLines += lines.size();
    }
    return totalLines;
}

std::vector<Line2D> LineDataAdapter::getLines() {
    std::vector<Line2D> allLines;
    
    // Collect all lines from all time frames
    for (auto const & [time, lines]: m_lineData->GetAllLinesAsRange()) {
        allLines.insert(allLines.end(), lines.begin(), lines.end());
    }
    
    return allLines;
}

std::vector<Line2D> LineDataAdapter::getLinesInRange(TimeFrameIndex start,
                                                      TimeFrameIndex end,
                                                      TimeFrame const * target_timeFrame) {
    // Use the LineData's built-in method to get lines in the time range
    // This method handles the time frame conversion internally
    auto lines_view = m_lineData->GetLinesInRange(TimeFrameInterval(start, end),
                                                  target_timeFrame,
                                                  m_timeFrame.get());

    // Convert the view to a vector
    std::vector<Line2D> result;
    for (auto const & timeLinesPair: lines_view) {
        result.insert(result.end(), timeLinesPair.lines.begin(), timeLinesPair.lines.end());
    }

    return result;
} 