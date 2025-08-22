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
    for (auto const & [time, points] : m_pointData->GetAllPointsAsRange()) {
        totalPoints += points.size();
    }
    return totalPoints;
}

std::vector<Point2D<float>> PointDataAdapter::getPoints() {
    std::vector<Point2D<float>> allPoints;
    allPoints.reserve(size());
    
    for (auto const & [time, points] : m_pointData->GetAllPointsAsRange()) {
        for (auto const & point : points) {
            allPoints.emplace_back(point.x, point.y);
        }
    }
    
    return allPoints;
}

std::vector<Point2D<float>> PointDataAdapter::getPointsInRange(TimeFrameIndex start,
                                                         TimeFrameIndex end,
                                                         TimeFrame const * target_timeFrame) {
    std::vector<Point2D<float>> rangePoints;

    // Get the source TimeFrame
    auto const * sourceTimeFrame = m_timeFrame.get();
    
    // Convert target range to source range if needed
    TimeFrameIndex sourceStart = start;
    TimeFrameIndex sourceEnd = end;
    
    if (target_timeFrame && sourceTimeFrame && target_timeFrame != sourceTimeFrame) {
        // Convert time range from target to source timeframe
        // This is a simplified conversion - in practice you might need more sophisticated mapping
        sourceStart = start;
        sourceEnd = end;
    }
    
    for (auto const & [time, points] : m_pointData->GetAllPointsAsRange()) {
        if (time >= sourceStart && time <= sourceEnd) {
            for (auto const & point : points) {
                rangePoints.emplace_back(point.x, point.y);
            }
        }
    }
    
    return rangePoints;
}

bool PointDataAdapter::hasMultiSamples() const {
    for (auto const & [time, points] : m_pointData->GetAllPointsAsRange()) {
        if (points.size() > 1) {
            return true;
        }
    }
    return false;
}

size_t PointDataAdapter::getEntityCountAt(TimeFrameIndex t) const {
    for (auto const & [time, points] : m_pointData->GetAllPointsAsRange()) {
        if (time == t) {
            return points.size();
        }
    }
    return 0;
}

Point2D<float> const* PointDataAdapter::getPointAt(TimeFrameIndex t, int entityIndex) const {
    for (auto const & [time, points] : m_pointData->GetAllPointsAsRange()) {
        if (time == t) {
            if (entityIndex >= 0 && static_cast<size_t>(entityIndex) < points.size()) {
                // Convert from internal point format to Point2D
                static thread_local Point2D<float> convertedPoint;
                convertedPoint.x = points[entityIndex].x;
                convertedPoint.y = points[entityIndex].y;
                return &convertedPoint;
            }
            break;
        }
    }
    return nullptr;
}

EntityId PointDataAdapter::getEntityIdAt(TimeFrameIndex t, int entityIndex) const {
    // For now, use the entity index as the ID
    // In the future, this could be extended to use actual entity IDs if PointData supports them
    return entityIndex;
}
