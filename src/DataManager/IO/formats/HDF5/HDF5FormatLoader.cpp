#include "HDF5FormatLoader.hpp"

#include "HDF5Loader.hpp"  // For the existing HDF5Loader implementation

#include <iostream>

LoadResult HDF5FormatLoader::load(std::string const& filepath, 
                                 IODataType dataType, 
                                 nlohmann::json const& config) const {
    switch (dataType) {
        case IODataType::Mask:
            return loadMaskDataHDF5(filepath, config);
            
        case IODataType::Line:
            return loadLineDataHDF5(filepath, config);
            
        default:
            return LoadResult("HDF5 loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool HDF5FormatLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support hdf5 format for MaskData and LineData
    if (format == "hdf5" && (dataType == IODataType::Mask || dataType == IODataType::Line)) {
        return true;
    }
    
    return false;
}

std::string HDF5FormatLoader::getLoaderName() const {
    return "HDF5FormatLoader";
}

LoadResult HDF5FormatLoader::loadMaskDataHDF5(std::string const& filepath, 
                                             nlohmann::json const& config) const {
    try {
        // Use the existing HDF5Loader functionality
        HDF5Loader hdf5_loader;
        return hdf5_loader.loadData(filepath, IODataType::Mask, config);
        
    } catch (std::exception const& e) {
        return LoadResult("HDF5 MaskData loading failed: " + std::string(e.what()));
    }
}

LoadResult HDF5FormatLoader::loadLineDataHDF5(std::string const& filepath, 
                                             nlohmann::json const& config) const {
    try {
        // Use the existing HDF5Loader functionality
        HDF5Loader hdf5_loader;
        return hdf5_loader.loadData(filepath, IODataType::Line, config);
        
    } catch (std::exception const& e) {
        return LoadResult("HDF5 LineData loading failed: " + std::string(e.what()));
    }
}
