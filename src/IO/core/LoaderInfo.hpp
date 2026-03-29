/**
 * @file LoaderInfo.hpp
 * @brief Metadata struct describing a registered loader's capabilities and schema.
 *
 * LoaderInfo is the loading counterpart of SaverInfo. Each IFormatLoader that
 * supports loading returns one LoaderInfo per (format, data_type) combination
 * it can handle, including a ParameterSchema that enables automatic UI
 * generation via AutoParamWidget.
 */
#ifndef DATAMANAGER_IO_LOADER_INFO_HPP
#define DATAMANAGER_IO_LOADER_INFO_HPP

#include "DataTypeEnum/DM_DataType.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <string>

/**
 * @brief Metadata describing a single loader capability within a format loader
 *
 * Each IFormatLoader that supports loading returns one LoaderInfo per
 * (format, data_type) combination it can handle.
 */
struct LoaderInfo {
    std::string format;                                    ///< Format identifier (e.g., "csv", "binary", "hdf5")
    DM_DataType data_type;                                 ///< Data type this loader produces
    std::string description;                               ///< Human-readable description (e.g., "CSV point data (frame, x, y)")
    bool supports_batch = false;                           ///< Whether batch loading is available for this combination
    WhiskerToolbox::Transforms::V2::ParameterSchema schema;///< Parameter schema for the loader options
};

#endif// DATAMANAGER_IO_LOADER_INFO_HPP
