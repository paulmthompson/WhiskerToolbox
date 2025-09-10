#include "LoaderRegistry.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

void LoaderRegistry::registerLoader(std::unique_ptr<IFormatLoader> loader) {
    if (!loader) {
        std::cerr << "LoaderRegistry: Attempted to register null loader" << std::endl;
        return;
    }
    
    //std::cout << "LoaderRegistry: Registered loader '" << loader->getLoaderName() << "'" << std::endl;
    m_loaders.push_back(std::move(loader));
}

LoadResult LoaderRegistry::tryLoad(std::string const& format, 
                                  IODataType dataType,
                                  std::string const& filepath, 
                                  nlohmann::json const& config, 
                                  DataFactory* factory) {
    if (!factory) {
        return LoadResult("DataFactory is null");
    }
    
    // Try each registered loader
    for (auto const& loader : m_loaders) {
        if (loader->supportsFormat(format, dataType)) {
            std::cout << "LoaderRegistry: Trying loader '" << loader->getLoaderName() 
                     << "' for format '" << format << "'" << std::endl;
            
            try {
                LoadResult result = loader->load(filepath, dataType, config, factory);
                if (result.success) {
                    std::cout << "LoaderRegistry: Successfully loaded with '" 
                             << loader->getLoaderName() << "'" << std::endl;
                    return result;
                } else {
                    std::cout << "LoaderRegistry: Loader '" << loader->getLoaderName() 
                             << "' failed: " << result.error_message << std::endl;
                }
            } catch (std::exception const& e) {
                std::cout << "LoaderRegistry: Loader '" << loader->getLoaderName() 
                         << "' threw exception: " << e.what() << std::endl;
            } catch (...) {
                std::cout << "LoaderRegistry: Loader '" << loader->getLoaderName() 
                         << "' threw unknown exception" << std::endl;
            }
        }
    }
    
    // No loader could handle this format
    std::ostringstream error_msg;
    error_msg << "No registered loader supports format '" << format 
              << "' for data type " << static_cast<int>(dataType);
    return LoadResult(error_msg.str());
}

LoadResult LoaderRegistry::trySave(std::string const& format, 
                                  IODataType dataType,
                                  std::string const& filepath, 
                                  nlohmann::json const& config, 
                                  void const* data) {
    if (!data) {
        return LoadResult("Data pointer is null");
    }
    
    // Try each registered loader
    for (auto const& loader : m_loaders) {
        if (loader->supportsFormat(format, dataType)) {
            std::cout << "LoaderRegistry: Trying to save with loader '" << loader->getLoaderName() 
                     << "' for format '" << format << "'" << std::endl;
            
            try {
                LoadResult result = loader->save(filepath, dataType, config, data);
                if (result.success) {
                    std::cout << "LoaderRegistry: Successfully saved with '" 
                             << loader->getLoaderName() << "'" << std::endl;
                    return result;
                } else {
                    std::cout << "LoaderRegistry: Saver '" << loader->getLoaderName() 
                             << "' failed: " << result.error_message << std::endl;
                }
            } catch (std::exception const& e) {
                std::cout << "LoaderRegistry: Saver '" << loader->getLoaderName() 
                         << "' threw exception: " << e.what() << std::endl;
            } catch (...) {
                std::cout << "LoaderRegistry: Saver '" << loader->getLoaderName() 
                         << "' threw unknown exception" << std::endl;
            }
        }
    }
    
    // No loader could handle this format for saving
    std::ostringstream error_msg;
    error_msg << "No registered loader supports saving format '" << format 
              << "' for data type " << static_cast<int>(dataType);
    return LoadResult(error_msg.str());
}

bool LoaderRegistry::isFormatSupported(std::string const& format, IODataType dataType) const {
    return std::any_of(m_loaders.begin(), m_loaders.end(),
                      [&](auto const& loader) {
                          return loader->supportsFormat(format, dataType);
                      });
}

std::vector<std::string> LoaderRegistry::getSupportedFormats(IODataType dataType) const {
    std::vector<std::string> formats;
    
    // This is a simplified implementation - in practice you might want to query each loader
    // for its supported formats to avoid hardcoding
    for (auto const& loader : m_loaders) {
        // For now, we'll need to check common formats
        // A more sophisticated approach would have loaders expose their supported formats
        std::vector<std::string> common_formats = {"csv", "capnp", "binary", "hdf5", "json", "image"};
        
        for (auto const& format : common_formats) {
            if (loader->supportsFormat(format, dataType)) {
                if (std::find(formats.begin(), formats.end(), format) == formats.end()) {
                    formats.push_back(format);
                }
            }
        }
    }
    
    return formats;
}

LoaderRegistry& LoaderRegistry::getInstance() {
    static LoaderRegistry instance;
    return instance;
}