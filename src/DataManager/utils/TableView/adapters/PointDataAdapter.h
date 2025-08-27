#ifndef POINT_DATA_ADAPTER_H
#define POINT_DATA_ADAPTER_H

#include "utils/TableView/interfaces/IPointSource.h"

#include <memory>
#include <vector>

class PointData;

/**
 * @brief Adapter that exposes PointData as an IPointSource.
 * 
 * This adapter provides a bridge between the existing PointData class
 * and the IPointSource interface required by the TableView system.
 * It handles PointData that may have multiple points per timestamp.
 */
class PointDataAdapter : public IPointSource {
public:
    /**
     * @brief Constructs a PointDataAdapter.
     * @param pointData Shared pointer to the PointData source.
     * @param timeFrame Shared pointer to the TimeFrame this data belongs to.
     * @param name The name of this data source.
     */
    PointDataAdapter(std::shared_ptr<PointData> pointData,
                     std::shared_ptr<TimeFrame> timeFrame,
                     std::string name);

    // IPointSource interface implementation
    [[nodiscard]] auto getName() const -> std::string const & override;
    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override;
    [[nodiscard]] auto size() const -> size_t override;
    std::vector<Point2D<float>> getPoints() override;
    std::vector<Point2D<float>> getPointsInRange(TimeFrameIndex start,
                                           TimeFrameIndex end,
                                           TimeFrame const * target_timeFrame) override;
    bool hasMultiSamples() const override;
    [[nodiscard]] auto getEntityCountAt(TimeFrameIndex t) const -> size_t override;
    [[nodiscard]] auto getPointAt(TimeFrameIndex t, int entityIndex) const -> Point2D<float> const* override;
    [[nodiscard]] EntityId getEntityIdAt(TimeFrameIndex t, int entityIndex) const override;

private:
    std::shared_ptr<PointData> m_pointData;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::string m_name;
};

#endif// POINT_DATA_ADAPTER_H
