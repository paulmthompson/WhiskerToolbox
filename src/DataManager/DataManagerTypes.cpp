#include "DataManagerTypes.hpp"


IODataType toIODataType(DM_DataType dm_type) {
    using enum DM_DataType;
    
    switch (dm_type) {
        case Video: return IODataType::Video;
        case Images: return IODataType::Images;
        case Points: return IODataType::Points;
        case Mask: return IODataType::Mask;
        case Line: return IODataType::Line;
        case Analog: return IODataType::Analog;
        case DigitalEvent: return IODataType::DigitalEvent;
        case DigitalInterval: return IODataType::DigitalInterval;
        case Tensor: return IODataType::Tensor;
        case Time: return IODataType::Time;
        case Unknown: return IODataType::Unknown;
    }
    return IODataType::Unknown;
}

DM_DataType fromIODataType(IODataType io_type) {
    using enum DM_DataType;
    
    switch (io_type) {
        case IODataType::Video: return Video;
        case IODataType::Images: return Images;
        case IODataType::Points: return Points;
        case IODataType::Mask: return Mask;
        case IODataType::Line: return Line;
        case IODataType::Analog: return Analog;
        case IODataType::DigitalEvent: return DigitalEvent;
        case IODataType::DigitalInterval: return DigitalInterval;
        case IODataType::Tensor: return Tensor;
        case IODataType::Time: return Time;
        case IODataType::Unknown: return Unknown;
    }
    return Unknown;
}
