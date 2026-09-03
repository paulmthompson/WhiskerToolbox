#include "HDF5FormatLoader.hpp"

#include "HDF5Loader.hpp"

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

        case DM_DataType::DigitalInterval:
            return loadDigitalIntervalDataHDF5(filepath, config);

        case DM_DataType::Analog:
            return loadAnalogDataHDF5(filepath, config);

        case DM_DataType::Time:
            return loadIdentityTimeFrameHDF5(filepath, config);

        default:
            return LoadResult("HDF5 loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool HDF5FormatLoader::supportsFormat(std::string const & format, DM_DataType dataType) const {
    if (format != "hdf5") {
        return false;
    }

    switch (dataType) {
        case DM_DataType::Mask:
        case DM_DataType::Line:
        case DM_DataType::DigitalEvent:
        case DM_DataType::DigitalInterval:
        case DM_DataType::Analog:
        case DM_DataType::Time:
            return true;
        default:
            return false;
    }
}

std::string HDF5FormatLoader::getLoaderName() const {
    return "HDF5FormatLoader";
}

LoadResult HDF5FormatLoader::loadMaskDataHDF5(std::string const & filepath,
                                              nlohmann::json const & config) {
    try {
        HDF5Loader const hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::Mask, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 MaskData loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadLineDataHDF5(std::string const & filepath,
                                              nlohmann::json const & config) {
    try {
        HDF5Loader const hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::Line, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 LineData loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadDigitalEventDataHDF5(std::string const & filepath,
                                                      nlohmann::json const & config) {
    try {
        HDF5Loader const hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::DigitalEvent, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 DigitalEventSeries loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadDigitalIntervalDataHDF5(std::string const & filepath,
                                                         nlohmann::json const & config) {
    try {
        HDF5Loader const hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::DigitalInterval, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 DigitalIntervalSeries loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadIdentityTimeFrameHDF5(std::string const & filepath,
                                                       nlohmann::json const & config) {
    try {
        HDF5Loader const hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::Time, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 TimeFrame loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadAnalogDataHDF5(std::string const & filepath,
                                                nlohmann::json const & config) {
    try {
        HDF5Loader const hdf5_loader;
        return hdf5_loader.loadData(filepath, DM_DataType::Analog, config);

    } catch (std::exception const & e) {
        return LoadResult("HDF5 AnalogTimeSeries loading failed: " + std::string(e.what()));
    }
}
