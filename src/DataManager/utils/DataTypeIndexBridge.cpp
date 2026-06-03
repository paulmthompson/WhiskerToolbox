/// @file DataTypeIndexBridge.cpp
/// @brief Bridge DM_DataType enum values to container std::type_index.

#include "DataTypeIndexBridge.hpp"

#include "ContainerTypeIndex.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <stdexcept>
#include <unordered_map>

namespace WhiskerToolbox::TypeTraits {

std::type_index dmDataTypeToContainerTypeIndex(DM_DataType type) {
    switch (type) {
        case DM_DataType::Analog:
            return TypeIndexMapper::stringToContainer("AnalogTimeSeries");
        case DM_DataType::RaggedAnalog:
            return TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries");
        case DM_DataType::Mask:
            return TypeIndexMapper::stringToContainer("MaskData");
        case DM_DataType::Line:
            return TypeIndexMapper::stringToContainer("LineData");
        case DM_DataType::Points:
            return TypeIndexMapper::stringToContainer("PointData");
        case DM_DataType::DigitalEvent:
            return TypeIndexMapper::stringToContainer("DigitalEventSeries");
        case DM_DataType::DigitalInterval:
            return TypeIndexMapper::stringToContainer("DigitalIntervalSeries");
        default:
            throw std::runtime_error(
                    "dmDataTypeToContainerTypeIndex: unsupported DM_DataType for pipeline source");
    }
}

std::optional<DM_DataType> containerTypeIndexToDmDataType(std::type_index container_type) {
    static std::unordered_map<std::type_index, DM_DataType> const map = {
            {typeid(MaskData), DM_DataType::Mask},
            {typeid(LineData), DM_DataType::Line},
            {typeid(PointData), DM_DataType::Points},
            {typeid(AnalogTimeSeries), DM_DataType::Analog},
            {typeid(RaggedAnalogTimeSeries), DM_DataType::RaggedAnalog},
            {typeid(DigitalEventSeries), DM_DataType::DigitalEvent},
            {typeid(DigitalIntervalSeries), DM_DataType::DigitalInterval},
    };

    auto const it = map.find(container_type);
    if (it != map.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace WhiskerToolbox::TypeTraits
