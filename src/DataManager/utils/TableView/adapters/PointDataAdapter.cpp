#include "PointDataAdapter.h"

#include "Points/Point_Data.hpp"

#include <stdexcept>

PointDataAdapter::PointDataAdapter(std::shared_ptr<PointData> pointData,
                                   std::shared_ptr<TimeFrame> timeFrame,
                                   std::string name)
    : m_pointData(std::move(pointData)),
      m_timeFrame(std::move(timeFrame)),
      m_name(std::move(name)) {
    if (!m_pointData) {
        throw std::invalid_argument("PointData cannot be null");
    }
}

std::string const & PointDataAdapter::getName() const {
    return m_name;
}

std::shared_ptr<TimeFrame> PointDataAdapter::getTimeFrame() const {
    return m_timeFrame;
}

size_t PointDataAdapter::size() const {
    size_t totalPoints = 0;
    for (auto const & [time, entries] : m_pointData->getAllEntries()) {
        totalPoints += entries.size();
    }
    return totalPoints;
}

std::vector<Point2D<float>> PointDataAdapter::getPoints() {
    std::vector<Point2D<float>> allPoints;
    allPoints.reserve(size());
    
    for (auto const & [time, entries] : m_pointData->getAllEntries()) {
        for (auto const & entry : entries) {
            allPoints.emplace_back(entry.data.x, entry.data.y);
        }
    }
    
    return allPoints;
}

std::vector<Point2D<float>> PointDataAdapter::getPointsInRange(TimeFrameIndex start,
                                                         TimeFrameIndex end,
                                                         TimeFrame const * target_timeFrame) {
    if (start == end) {
        auto const & points_ref = m_pointData->getAtTime(start, *target_timeFrame);
        return {points_ref.begin(), points_ref.end()};
    }

    // Use the LineData's built-in method to get lines in the time range.
    // This method handles the time frame conversion internally via a lazy view.
    auto points_view = m_pointData->GetEntriesInRange(TimeFrameInterval(start, end),
                                                  *target_timeFrame);
    
    // Materialize the view into a vector
    std::vector<Point2D<float>> result;
    for (auto const & [time, entries]: points_view) {
        for (auto const & entry: entries) {
            result.push_back(entry.data);
        }
    }

    return result;
}

bool PointDataAdapter::hasMultiSamples() const {
    for (auto const & [time, entries] : m_pointData->getAllEntries()) {
        if (entries.size() > 1) {
            return true;
        }
    }
    return false;
}

size_t PointDataAdapter::getEntityCountAt(TimeFrameIndex t) const {
    auto const & points_ref = m_pointData->getAtTime(t, *m_timeFrame);
    return points_ref.size();
}

Point2D<float> const* PointDataAdapter::getPointAt(TimeFrameIndex t, int entityIndex) const {
    auto const & points_ref = m_pointData->getAtTime(t, *m_timeFrame);
    if (entityIndex < 0 || static_cast<size_t>(entityIndex) >= points_ref.size()) {
        return nullptr;
    }
    return &points_ref[static_cast<size_t>(entityIndex)];
}

EntityId PointDataAdapter::getEntityIdAt(TimeFrameIndex, int entityIndex) const {
    // For now, use the entity index as the ID
    // In the future, this could be extended to use actual entity IDs if PointData supports them
    return static_cast<EntityId>(entityIndex);
}
