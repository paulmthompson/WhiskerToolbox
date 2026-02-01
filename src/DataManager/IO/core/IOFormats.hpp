#ifndef DATAMANAGER_IO_FORMATS_HPP
#define DATAMANAGER_IO_FORMATS_HPP

#include <string>
#include <string_view>

/**
 * @brief Unified format enumeration for all data loading/saving operations
 * 
 * This enum replaces the per-data-type format enums (AnalogDataType, EventDataType,
 * IntervalDataType) with a single consistent enumeration. Format names are
 * standardized to use consistent casing and naming conventions.
 * 
 * @note Not all formats apply to all data types. For example:
 *   - Binary: AnalogTimeSeries, DigitalEventSeries, DigitalIntervalSeries
 *   - CSV: All data types
 *   - CapnProto: LineData (others could be added)
 *   - HDF5: LineData, MaskData (others could be added)
 *   - Images: MaskData
 *   - DLC_CSV: PointData (DeepLabCut format)
 *   - MultiColumnBinary: DigitalIntervalSeries, TimeFrame
 */
enum class IOFormat {
    Binary,             ///< Raw binary file (various underlying data types: int16, float32, etc.)
    CSV,                ///< Comma-separated values (or custom delimiter)
    CapnProto,          ///< Cap'n Proto serialization format
    HDF5,               ///< HDF5 hierarchical data format
    Images,             ///< Image files (PNG, TIFF, etc.) via OpenCV
    DLC_CSV,            ///< DeepLabCut CSV format for point data
    MultiColumnBinary,  ///< Multi-column binary with column mapping (e.g., timestamps + data)
    JSON,               ///< JSON format (for configuration or simple data)
    Unknown             ///< Unrecognized format
};

/**
 * @brief Convert IOFormat enum to string representation
 * 
 * @param format The format enum value
 * @return String representation matching JSON format field values
 */
inline std::string toString(IOFormat format) {
    switch (format) {
        case IOFormat::Binary: return "binary";
        case IOFormat::CSV: return "csv";
        case IOFormat::CapnProto: return "capnp";
        case IOFormat::HDF5: return "hdf5";
        case IOFormat::Images: return "images";
        case IOFormat::DLC_CSV: return "dlc_csv";
        case IOFormat::MultiColumnBinary: return "multi_column_binary";
        case IOFormat::JSON: return "json";
        case IOFormat::Unknown: return "unknown";
    }
    return "unknown";
}

/**
 * @brief Parse a format string to IOFormat enum
 * 
 * Supports multiple aliases for backward compatibility:
 *   - "binary", "int16" (legacy) -> Binary
 *   - "csv" -> CSV
 *   - "capnp", "capnproto" -> CapnProto
 *   - "hdf5" -> HDF5
 *   - "images", "opencv" -> Images
 *   - "dlc_csv", "deeplabcut" -> DLC_CSV
 *   - "multi_column_binary" -> MultiColumnBinary
 *   - "uint16" (legacy for digital data) -> Binary
 * 
 * @param format_str The format string from JSON or user input
 * @return IOFormat enum value, or IOFormat::Unknown if not recognized
 */
inline IOFormat parseFormat(std::string_view format_str) {
    // Primary format names
    if (format_str == "binary") return IOFormat::Binary;
    if (format_str == "csv") return IOFormat::CSV;
    if (format_str == "capnp" || format_str == "capnproto") return IOFormat::CapnProto;
    if (format_str == "hdf5") return IOFormat::HDF5;
    if (format_str == "images" || format_str == "opencv") return IOFormat::Images;
    if (format_str == "dlc_csv" || format_str == "deeplabcut") return IOFormat::DLC_CSV;
    if (format_str == "multi_column_binary") return IOFormat::MultiColumnBinary;
    if (format_str == "json") return IOFormat::JSON;
    
    // Legacy aliases for backward compatibility
    if (format_str == "int16") return IOFormat::Binary;  // Old analog format name
    if (format_str == "uint16") return IOFormat::Binary; // Old digital format name
    
    return IOFormat::Unknown;
}

/**
 * @brief Check if a format is supported for a given data type
 * 
 * This is a compile-time helper that documents which formats work with which data types.
 * Runtime checks should use LoaderRegistry::isFormatSupported().
 * 
 * @note This function is not implemented - use LoaderRegistry for runtime checks.
 */

#endif // DATAMANAGER_IO_FORMATS_HPP
