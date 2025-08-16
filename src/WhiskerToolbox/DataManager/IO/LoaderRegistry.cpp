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

DataLoader const* LoaderRegistry::findLoader(std::string const& format_id, DM_DataType data_type) const {
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

std::vector<DM_DataType> LoaderRegistry::getSupportedDataTypes(std::string const& format_id) const {
    auto it = _loaders.find(format_id);
    if (it == _loaders.end()) {
        return {};
    }
    
    std::vector<DM_DataType> supported_types;
    
    // Check all possible data types
    std::vector<DM_DataType> all_types = {
        DM_DataType::Line,
        DM_DataType::Points,
        DM_DataType::Mask,
        DM_DataType::Images,
        DM_DataType::Video,
        DM_DataType::Analog,
        DM_DataType::DigitalEvent,
        DM_DataType::DigitalInterval,
        DM_DataType::Tensor,
        DM_DataType::Time
    };
    
    for (DM_DataType type : all_types) {
        if (it->second->supportsDataType(type)) {
            supported_types.push_back(type);
        }
    }
    
    return supported_types;
}
