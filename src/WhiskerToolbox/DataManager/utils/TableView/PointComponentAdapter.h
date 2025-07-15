#ifndef POINT_COMPONENT_ADAPTER_H
#define POINT_COMPONENT_ADAPTER_H

#include "IAnalogSource.h"
#include "Points/Point_Data.hpp"

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
        X,  ///< X component of points
        Y   ///< Y component of points
    };

    /**
     * @brief Constructs a PointComponentAdapter.
     * @param pointData Shared pointer to the PointData source.
     * @param component The component (X or Y) to extract.
     * @param timeFrameId The TimeFrame ID this data belongs to.
     */
    PointComponentAdapter(std::shared_ptr<PointData> pointData, 
                         Component component, 
                         int timeFrameId);

    // IAnalogSource interface implementation
    [[nodiscard]] auto getTimeFrameId() const -> int override;
    [[nodiscard]] auto size() const -> size_t override;
    auto getDataSpan() -> std::span<const double> override;

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
    int m_timeFrameId;
    std::vector<double> m_materializedData;
    bool m_isMaterialized = false;
};

#endif // POINT_COMPONENT_ADAPTER_H
