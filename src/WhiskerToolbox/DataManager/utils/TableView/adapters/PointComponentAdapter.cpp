#include "PointComponentAdapter.h"

#include <algorithm>
#include <stdexcept>

PointComponentAdapter::PointComponentAdapter(std::shared_ptr<PointData> pointData, 
                                           Component component, 
                                           int timeFrameId,
                                           std::string name)
    : m_pointData(std::move(pointData))
    , m_component(component)
    , m_timeFrameId(timeFrameId)
    , m_name(std::move(name))
{
    if (!m_pointData) {
        throw std::invalid_argument("PointData cannot be null");
    }
}

const std::string& PointComponentAdapter::getName() const {
    return m_name;
}

int PointComponentAdapter::getTimeFrameId() const {
    return m_timeFrameId;
}

size_t PointComponentAdapter::size() const {
    if (!m_isMaterialized) {
        // Calculate size without materializing
        size_t totalPoints = 0;
        for (auto const& [time, points] : m_pointData->GetAllPointsAsRange()) {
            totalPoints += points.size();
        }
        return totalPoints;
    }
    return m_materializedData.size();
}

std::span<const double> PointComponentAdapter::getDataSpan() {
    if (!m_isMaterialized) {
        materializeData();
    }
    return std::span<const double>(m_materializedData);
}

std::vector<float> PointComponentAdapter::getDataInRange(TimeFrameIndex start,
                                                           TimeFrameIndex end,
                                                           TimeFrame const & source_timeFrame,
                                                           TimeFrame const & target_timeFrame) 
{
    auto point_range = m_pointData->GetPointsInRange(TimeFrameInterval(start, end), 
                                                     &source_timeFrame, 
                                                     &target_timeFrame);
    if (point_range.empty()) {
        return {};
    }
    std::vector<float> componentValues;

    for (const auto& time_point_pair : point_range) {
        float componentValue = (m_component == Component::X) ? time_point_pair.points[0].x : time_point_pair.points[0].y;
        componentValues.push_back(componentValue);
    }
    return componentValues;
}

void PointComponentAdapter::materializeData() {
    if (m_isMaterialized) {
        return;
    }

    // Reserve space for efficiency
    m_materializedData.clear();
    m_materializedData.reserve(size());

    // Collect all points in time order and extract the specified component
    std::vector<std::pair<TimeFrameIndex, std::vector<Point2D<float>> const*>> timePointPairs;
    
    for (auto const& [time, points] : m_pointData->GetAllPointsAsRange()) {
        timePointPairs.emplace_back(time, &points);
    }

    // Sort by time to ensure consistent ordering
    std::sort(timePointPairs.begin(), timePointPairs.end(),
              [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });

    // Extract the component values
    for (const auto& [time, points] : timePointPairs) {
        for (const auto& point : *points) {
            double componentValue = (m_component == Component::X) ? 
                                   static_cast<double>(point.x) : 
                                   static_cast<double>(point.y);
            m_materializedData.push_back(componentValue);
        }
    }

    m_isMaterialized = true;
}
