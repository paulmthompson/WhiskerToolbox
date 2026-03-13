#include "NumpyFormatLoader.hpp"

#include "NumpyLoader.hpp"

#include <iostream>

LoadResult NumpyFormatLoader::load(std::string const& filepath, 
                                   IODataType dataType, 
                                   nlohmann::json const& config) const {
    switch (dataType) {
        case IODataType::Tensor:
            return loadTensorDataNumpy(filepath, config);
            
        default:
            return LoadResult("Numpy loader does not support data type: " + 
                            std::to_string(static_cast<int>(dataType)));
    }
}

bool NumpyFormatLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support numpy format for TensorData
    if (format == "numpy" && dataType == IODataType::Tensor) {
        return true;
    }
    
    return false;
}

std::string NumpyFormatLoader::getLoaderName() const {
    return "NumpyFormatLoader";
}

LoadResult NumpyFormatLoader::loadTensorDataNumpy(std::string const& filepath, 
                                                   nlohmann::json const& config) const {
    try {
        NumpyLoader numpy_loader;
        return numpy_loader.loadData(filepath, IODataType::Tensor, config);
        
    } catch (std::exception const& e) {
        return LoadResult("Numpy TensorData loading failed: " + std::string(e.what()));
    }
}
