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
    for (auto const & [time, entries]: m_lineData->getAllEntries()) {
        totalLines += entries.size();
    }
    return totalLines;
}

auto LineDataAdapter::getLines() -> std::vector<Line2D> {
    std::vector<Line2D> allLines;

    // Collect all lines from all time frames
    for (auto const & [time, entries]: m_lineData->getAllEntries()) {
        for (auto const & entry: entries) {
            allLines.push_back(entry.data);
        }
    }

    return allLines;
}

auto LineDataAdapter::getLinesInRange(TimeFrameIndex start,
                                      TimeFrameIndex end,
                                      TimeFrame const * target_timeFrame) -> std::vector<Line2D> {
    // Fast path: when start == end, avoid constructing a ranges pipeline.
    // Directly fetch the lines at the single time index using timeframe conversion.
    if (start == end) {
        auto const & lines_ref = m_lineData->getAtTime(start, *target_timeFrame);
        return {lines_ref.begin(), lines_ref.end()};
    }

    // Use the LineData's built-in method to get lines in the time range.
    // This method handles the time frame conversion internally via a lazy view.
    auto lines_view = m_lineData->GetEntriesInRange(TimeFrameInterval(start, end),
                                                  *target_timeFrame);

    // Materialize the view into a vector
    std::vector<Line2D> result;
    for (auto const & [time, entries]: lines_view) {
        for (auto const & entry: entries) {
            result.push_back(entry.data);
        }
    }

    return result;
}

bool LineDataAdapter::hasMultiSamples() const {
    // Check if any timestamp has more than one line
    for (auto const & [time, entries]: m_lineData->getAllEntries()) {
        if (entries.size() > 1) {
            return true;
        }
    }
    return false;
}

auto LineDataAdapter::getEntityCountAt(TimeFrameIndex t) const -> size_t {
    auto const & lines_ref = m_lineData->getAtTime(t, *m_timeFrame);
    return lines_ref.size();
}

auto LineDataAdapter::getLineAt(TimeFrameIndex t, int entityIndex) const -> Line2D const * {
    auto const & lines_ref = m_lineData->getAtTime(t, *m_timeFrame);
    if (entityIndex < 0 || static_cast<size_t>(entityIndex) >= lines_ref.size()) {
        return nullptr;
    }
    return &lines_ref[static_cast<size_t>(entityIndex)];
}

EntityId LineDataAdapter::getEntityIdAt(TimeFrameIndex t, int entityIndex) const {
    auto const & ids = m_lineData->getEntityIdsAtTime(t);
    if (entityIndex < 0 || static_cast<size_t>(entityIndex) >= ids.size()) return EntityId(0);
    return ids[static_cast<size_t>(entityIndex)];
}

std::vector<EntityId> LineDataAdapter::getEntityIdsAtTime(TimeFrameIndex t,
                                                          TimeFrame const * target_timeframe) const {
    auto const & ids_view = m_lineData->getEntityIdsAtTime(t, *target_timeframe);
    std::vector<EntityId> ids(ids_view.begin(), ids_view.end());
    return ids;
}