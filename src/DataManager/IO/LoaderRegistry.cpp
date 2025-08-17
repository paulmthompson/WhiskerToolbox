#include "LoaderRegistry.hpp"
#include <iostream>

LoaderRegistry& LoaderRegistry::getInstance() {
    static LoaderRegistry instance;
    return instance;
}

bool LoaderRegistry::registerLoader(std::unique_ptr<DataLoader> loader) {
    if (!loader) {
        std::cerr << "LoaderRegistry: Attempted to register null loader" << std::endl;
        return false;
    }
    
    std::string const format_id = loader->getFormatId();
    
    if (_loaders.find(format_id) != _loaders.end()) {
        std::cerr << "LoaderRegistry: Format '" << format_id 
                  << "' is already registered" << std::endl;
        return false;
    }
    
    std::cout << "LoaderRegistry: Registered loader for format '" << format_id << "'" << std::endl;
    _loaders[format_id] = std::move(loader);
    return true;
}

DataLoader const* LoaderRegistry::findLoader(std::string const& format_id, IODataType data_type) const {
    auto it = _loaders.find(format_id);
    if (it == _loaders.end()) {
        return nullptr;
    }
    
    if (!it->second->supportsDataType(data_type)) {
        return nullptr;
    }
    
    return it->second.get();
}

std::vector<std::string> LoaderRegistry::getRegisteredFormats() const {
    std::vector<std::string> formats;
    formats.reserve(_loaders.size());
    
    for (auto const& [format_id, loader] : _loaders) {
        formats.push_back(format_id);
    }
    
    return formats;
}

std::vector<IODataType> LoaderRegistry::getSupportedDataTypes(std::string const& format_id) const {
    auto it = _loaders.find(format_id);
    if (it == _loaders.end()) {
        return {};
    }
    
    std::vector<IODataType> supported_types;
    
    // Check all possible data types
    using enum IODataType;
    std::vector<IODataType> all_types = {
        Line, Points, Mask, Images, Video,
        Analog, DigitalEvent, DigitalInterval, Tensor, Time
    };
    
    for (IODataType type : all_types) {
        if (it->second->supportsDataType(type)) {
            supported_types.push_back(type);
        }
    }
    
    return supported_types;
}
