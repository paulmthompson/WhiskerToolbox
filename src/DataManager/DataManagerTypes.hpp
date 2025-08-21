#ifndef DATAMANAGERTYPES_HPP
#define DATAMANAGERTYPES_HPP

#include "IO/interface/IOTypes.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class LineData;
class MaskData;
class MediaData;
class PointData;
class TensorData;

enum class DM_DataType {
    Video,
    Images,
    Points,
    Mask,
    Line,
    Analog,
    DigitalEvent,
    DigitalInterval,
    Tensor,
    Time,
    Unknown
};

/**
 * @brief Convert from DM_DataType to IODataType
 */
IODataType toIODataType(DM_DataType dm_type);

/**
 * @brief Convert from IODataType to DM_DataType
 */
DM_DataType fromIODataType(IODataType io_type);

using DataTypeVariant = std::variant<
        std::shared_ptr<MediaData>,
        std::shared_ptr<PointData>,
        std::shared_ptr<LineData>,
        std::shared_ptr<MaskData>,
        std::shared_ptr<AnalogTimeSeries>,
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
