#ifndef DATAMANAGER_IO_TYPES_HPP
#define DATAMANAGER_IO_TYPES_HPP

/**
 * @brief Data types supported by the IO system
 * 
 * This enum is independent of DataManager and can be used by IO plugins.
 */
enum class IODataType {
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

#endif // DATAMANAGER_IO_TYPES_HPP
