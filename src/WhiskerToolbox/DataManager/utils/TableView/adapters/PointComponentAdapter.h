#ifndef POINT_COMPONENT_ADAPTER_H
#define POINT_COMPONENT_ADAPTER_H

#include "Points/Point_Data.hpp"
#include "utils/TableView/interfaces/IAnalogSource.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Adapter that exposes a component (x or y) of PointData as an analog source.
 * 
 * This adapter implements the IAnalogSource interface to provide access to either
 * the x or y components of a PointData object as a contiguous series of doubles.
 * It performs lazy, one-time materialization of the component data.
 */
class PointComponentAdapter : public IAnalogSource {
public:
    /**
     * @brief Component type enumeration.
     */
    enum class Component : std::uint8_t {
        X,///< X component of points
        Y ///< Y component of points
    };

    /**
     * @brief Constructs a PointComponentAdapter.
     * @param pointData Shared pointer to the PointData source.
     * @param component The component (X or Y) to extract.
     * @param timeFrameId The TimeFrame ID this data belongs to.
     * @param name The name of this data source.
     */
    PointComponentAdapter(std::shared_ptr<PointData> pointData,
                          Component component,
                          std::shared_ptr<TimeFrame> timeFrame,
                          std::string name);

    // IAnalogSource interface implementation

    /**
     * @brief Gets the name of this data source.
     * 
     * This name is used for dependency tracking and ExecutionPlan caching
     * in the TableView system.
     * 
     * @return The name of the data source.
     */
    [[nodiscard]] auto getName() const -> std::string const & override;

    /**
     * @brief Gets the TimeFrame the data belongs to.
     * @return A shared pointer to the TimeFrame.
     */
    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override;

    /**
     * @brief Gets the total number of samples in the source.
     * @return The number of samples.
     */
    [[nodiscard]] auto size() const -> size_t override;

    /**
     * @brief Gets the data in a specified range.
     * 
     * This method retrieves the component data for the specified time range
     * and returns it as a vector of floats.
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of floats containing the component data in the specified range.
     */
    std::vector<float> getDataInRange(TimeFrameIndex start,
                                      TimeFrameIndex end,
                                      TimeFrame const * target_timeFrame) override;

private:
    /**
     * @brief Materializes the component data if not already done.
     * 
     * This method performs the lazy, one-time gathering of the specified
     * component from all points in the PointData source into a contiguous
     * std::vector<double>.
     */
    void materializeData();

    std::shared_ptr<PointData> m_pointData;
    Component m_component;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::string m_name;
    std::vector<double> m_materializedData;
    bool m_isMaterialized = false;
};

#endif// POINT_COMPONENT_ADAPTER_H
