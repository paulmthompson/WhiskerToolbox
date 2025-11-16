#ifndef DATAMANAGERFWD_HPP
#define DATAMANAGERFWD_HPP

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

#endif // DATAMANAGERFWD_HPP