#include "HDF5FormatLoader.hpp"

#include "HDF5Loader.hpp"// For the existing HDF5Loader implementation

#include <iostream>

LoadResult HDF5FormatLoader::load(std::string const & filepath,
                                  DM_DataType dataType,
                                  nlohmann::json const & config) const {
    switch (dataType) {
        case DM_DataType::Mask:
            return loadMaskDataHDF5(filepath, config);

        case DM_DataType::Line:
            return loadLineDataHDF5(filepath, config);

        case DM_DataType::DigitalEvent:
            return loadDigitalEventDataHDF5(filepath, config);

        case DM_DataType::Analog:
            return loadAnalogDataHDF5(filepath, config);

        default:
            return LoadResult("HDF5 loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool HDF5FormatLoader::supportsFormat(std::string const & format, DM_DataType dataType) const {
    // Support hdf5 format for MaskData, LineData, DigitalEventSeries, and AnalogTimeSeries
    if (format == "hdf5" &&
        (dataType == DM_DataType::Mask ||
         dataType == DM_DataType::Line ||
         dataType == DM_DataType::DigitalEvent ||
         dataType == DM_DataType::Analog)) {
        return true;
    }

    return false;
}

std::string HDF5FormatLoader::getLoaderName() const {
    return "HDF5FormatLoader";
}

LoadResult HDF5FormatLoader::loadMaskDataHDF5(std::string const & filepath,
                                              nlohmann::json const & config) const {
    try {
        // Use the existing HDF5Loader functionality
        HDF5Loader hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::Mask, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 MaskData loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadLineDataHDF5(std::string const & filepath,
                                              nlohmann::json const & config) const {
    try {
        // Use the existing HDF5Loader functionality
        HDF5Loader hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::Line, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 LineData loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadDigitalEventDataHDF5(std::string const & filepath,
                                                      nlohmann::json const & config) const {
    try {
        // Use the HDF5Loader functionality
        HDF5Loader hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::DigitalEvent, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 DigitalEventSeries loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadAnalogDataHDF5(std::string const & filepath,
                                                nlohmann::json const & config) const {
    try {
        // Use the HDF5Loader functionality
        HDF5Loader hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::Analog, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 AnalogTimeSeries loading failed: " + std::string(e.what()));
    }
}
