#ifndef DATAMANAGERTYPES_HPP
#define DATAMANAGERTYPES_HPP


#include "DataManager/DataManagerFwd.hpp"
#include "IO/core/IOTypes.hpp"
#include "datamanager_export.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>


/**
 * @brief Convert from DM_DataType to IODataType
 * 
 * This conversion is constexpr and provides compile-time verification
 * that the two enums stay in sync. RaggedAnalog maps to Analog since
 * IODataType doesn't distinguish between regular and ragged analog.
 */
constexpr IODataType toIODataType(DM_DataType dm_type) noexcept {
    switch (dm_type) {
        case DM_DataType::Video: return IODataType::Video;
        case DM_DataType::Images: return IODataType::Images;
        case DM_DataType::Points: return IODataType::Points;
        case DM_DataType::Mask: return IODataType::Mask;
        case DM_DataType::Line: return IODataType::Line;
        case DM_DataType::Analog: return IODataType::Analog;
        case DM_DataType::RaggedAnalog: return IODataType::Analog;  // RaggedAnalog uses same IO
        case DM_DataType::DigitalEvent: return IODataType::DigitalEvent;
        case DM_DataType::DigitalInterval: return IODataType::DigitalInterval;
        case DM_DataType::Tensor: return IODataType::Tensor;
        case DM_DataType::Time: return IODataType::Time;
        case DM_DataType::Unknown: return IODataType::Unknown;
    }
    return IODataType::Unknown;
}

/**
 * @brief Convert from IODataType to DM_DataType
 * 
 * This conversion is constexpr. Note that IODataType::Analog maps to
 * DM_DataType::Analog (not RaggedAnalog) since the distinction is
 * made at a higher level.
 */
constexpr DM_DataType fromIODataType(IODataType io_type) noexcept {
    switch (io_type) {
        case IODataType::Video: return DM_DataType::Video;
        case IODataType::Images: return DM_DataType::Images;
        case IODataType::Points: return DM_DataType::Points;
        case IODataType::Mask: return DM_DataType::Mask;
        case IODataType::Line: return DM_DataType::Line;
        case IODataType::Analog: return DM_DataType::Analog;
        case IODataType::DigitalEvent: return DM_DataType::DigitalEvent;
        case IODataType::DigitalInterval: return DM_DataType::DigitalInterval;
        case IODataType::Tensor: return DM_DataType::Tensor;
        case IODataType::Time: return DM_DataType::Time;
        case IODataType::Unknown: return DM_DataType::Unknown;
    }
    return DM_DataType::Unknown;
}

// Compile-time verification that enum values match (where applicable)
static_assert(toIODataType(DM_DataType::Video) == IODataType::Video);
static_assert(toIODataType(DM_DataType::Line) == IODataType::Line);
static_assert(toIODataType(DM_DataType::Analog) == IODataType::Analog);
static_assert(fromIODataType(IODataType::Video) == DM_DataType::Video);
static_assert(fromIODataType(IODataType::Line) == DM_DataType::Line);
static_assert(fromIODataType(IODataType::Analog) == DM_DataType::Analog);

using DataTypeVariant = std::variant<
        std::shared_ptr<MediaData>,
        std::shared_ptr<PointData>,
        std::shared_ptr<LineData>,
        std::shared_ptr<MaskData>,
        std::shared_ptr<AnalogTimeSeries>,
        std::shared_ptr<RaggedAnalogTimeSeries>,
        std::shared_ptr<DigitalEventSeries>,
        std::shared_ptr<DigitalIntervalSeries>,
        std::shared_ptr<TensorData>>;

struct DataInfo {
    std::string key;
    std::string data_class;
    std::string color;
};

struct DataGroup {
    std::string groupName;
    std::vector<std::string> dataKeys;
};


#endif// DATAMANAGERTYPES_HPP
