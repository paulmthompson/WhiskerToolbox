#include "ContainerTraits.hpp"

// Include actual definitions
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <stdexcept>

namespace WhiskerToolbox::Transforms::V2 {

std::type_index TypeIndexMapper::elementToContainer(std::type_index element_type) {
    static std::unordered_map<std::type_index, std::type_index> const map = {
        {typeid(Mask2D), typeid(MaskData)},
        {typeid(Line2D), typeid(LineData)},
        {typeid(Point2D<float>), typeid(PointData)},
        {typeid(float), typeid(RaggedAnalogTimeSeries)}
    };
    
    auto it = map.find(element_type);
    if (it != map.end()) {
        return it->second;
    }
    
    throw std::runtime_error("Unknown element type in mapping");
}

std::type_index TypeIndexMapper::containerToElement(std::type_index container_type) {
    static std::unordered_map<std::type_index, std::type_index> const map = {
        {typeid(MaskData), typeid(Mask2D)},
        {typeid(LineData), typeid(Line2D)},
        {typeid(PointData), typeid(Point2D<float>)},
        {typeid(RaggedAnalogTimeSeries), typeid(float)}
    };
    
    auto it = map.find(container_type);
    if (it != map.end()) {
        return it->second;
    }
    
    throw std::runtime_error("Unknown container type in mapping");
}

std::string TypeIndexMapper::containerToString(std::type_index container_type) {
    static std::unordered_map<std::type_index, std::string> const map = {
        {typeid(MaskData), "MaskData"},
        {typeid(LineData), "LineData"},
        {typeid(PointData), "PointData"},
        {typeid(AnalogTimeSeries), "AnalogTimeSeries"},
        {typeid(RaggedAnalogTimeSeries), "RaggedAnalogTimeSeries"},
        {typeid(DigitalEventSeries), "DigitalEventSeries"},
        {typeid(DigitalIntervalSeries), "DigitalIntervalSeries"}
    };
    
    auto it = map.find(container_type);
    if (it != map.end()) {
        return it->second;
    }
    
    return "Unknown";
}

std::type_index TypeIndexMapper::stringToContainer(std::string const& name) {
    static std::unordered_map<std::string, std::type_index> const map = {
        {"MaskData", typeid(MaskData)},
        {"LineData", typeid(LineData)},
        {"PointData", typeid(PointData)},
        {"AnalogTimeSeries", typeid(AnalogTimeSeries)},
        {"RaggedAnalogTimeSeries", typeid(RaggedAnalogTimeSeries)},
        {"DigitalEventSeries", typeid(DigitalEventSeries)},
        {"DigitalIntervalSeries", typeid(DigitalIntervalSeries)}
    };
    
    auto it = map.find(name);
    if (it != map.end()) {
        return it->second;
    }
    
    throw std::runtime_error("Unknown container name: " + name);
}

} // namespace WhiskerToolbox::Transforms::V2
