#ifndef IPOINT_SOURCE_H
#define IPOINT_SOURCE_H

#include "TimeFrame/TimeFrame.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityTypes.hpp"

#include <memory>
#include <string>
#include <vector>


/**
 * @brief Interface for accessing point data sources in the TableView system.
 * 
 * This interface provides access to point data that may have multiple points
 * per timestamp. It follows the same pattern as ILineSource for multi-entity
 * data handling.
 */
class IPointSource {
public:
    virtual ~IPointSource() = default;

    /**
     * @brief Gets the name of this data source.
     * @return The name of the data source.
     */
    virtual std::string const & getName() const = 0;

    /**
     * @brief Gets the TimeFrame the data belongs to.
     * @return A shared pointer to the TimeFrame.
     */
    virtual std::shared_ptr<TimeFrame> getTimeFrame() const = 0;

    /**
     * @brief Gets the total number of points across all timestamps.
     * @return The total number of points.
     */
    virtual size_t size() const = 0;

    /**
     * @brief Gets all points from all timestamps.
     * @return A vector containing all points.
     */
    virtual std::vector<Point2D<float>> getPoints() = 0;

    /**
     * @brief Gets points within a specific time range.
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame for the data.
     * @return A vector of points in the specified range.
     */
    virtual std::vector<Point2D<float>> getPointsInRange(TimeFrameIndex start,
                                                   TimeFrameIndex end,
                                                   TimeFrame const * target_timeFrame) = 0;

    /**
     * @brief Checks if this source has multiple samples at any timestamp.
     * @return True if any timestamp has more than one point, false otherwise.
     */
    virtual bool hasMultiSamples() const = 0;

    /**
     * @brief Gets the number of points at a specific timestamp.
     * @param t The time frame index.
     * @return The number of points at that timestamp.
     */
    virtual size_t getEntityCountAt(TimeFrameIndex t) const = 0;

    /**
     * @brief Gets a specific point at a timestamp by entity index.
     * @param t The time frame index.
     * @param entityIndex The index of the entity at that timestamp.
     * @return Pointer to the point, or nullptr if not found.
     */
    virtual Point2D<float> const* getPointAt(TimeFrameIndex t, int entityIndex) const = 0;

    /**
     * @brief Gets the entity ID for a point at a timestamp.
     * @param t The time frame index.
     * @param entityIndex The index of the entity at that timestamp.
     * @return The entity ID.
     */
    virtual EntityId getEntityIdAt(TimeFrameIndex t, int entityIndex) const = 0;
};

#endif// IPOINT_SOURCE_H
