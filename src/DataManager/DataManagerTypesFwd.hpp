#ifndef DATAMANAGERTYPES_FWD_HPP
#define DATAMANAGERTYPES_FWD_HPP

// Forward declarations for DataManager types
// This header provides lightweight forward declarations to reduce
// compilation dependencies. Include this instead of DataManagerTypes.hpp
// when you only need forward declarations.

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

#endif// DATAMANAGERTYPES_FWD_HPP
