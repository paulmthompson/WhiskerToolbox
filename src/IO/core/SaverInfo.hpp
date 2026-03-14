/**
 * @file SaverInfo.hpp
 * @brief Metadata struct describing a registered saver's capabilities and schema.
 */
#ifndef DATAMANAGER_IO_SAVER_INFO_HPP
#define DATAMANAGER_IO_SAVER_INFO_HPP

#include "DataTypeEnum/DM_DataType.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <string>

/**
 * @brief Metadata describing a single saver capability within a format loader
 *
 * Each IFormatLoader that supports saving returns one SaverInfo per
 * (format, data_type) combination it can handle.
 */
struct SaverInfo {
    std::string format;                                    ///< Format identifier (e.g., "csv", "capnproto", "opencv")
    DM_DataType data_type;                                 ///< Data type this saver handles
    std::string description;                               ///< Human-readable description (e.g., "CSV point data (frame, x, y)")
    WhiskerToolbox::Transforms::V2::ParameterSchema schema;///< Parameter schema for the saver options
};

#endif// DATAMANAGER_IO_SAVER_INFO_HPP
