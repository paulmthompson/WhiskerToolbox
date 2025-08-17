#include "PluginLoader.hpp"
#include "LoaderRegistry.hpp"

#include <iostream>

LoadResult PluginLoader::loadData(
    std::string const& file_path,
    IODataType data_type,
    nlohmann::json const& config,
    DataFactory* factory
) {
    if (!factory) {
        return LoadResult("DataFactory is null");
    }
    
    // Extract format from config
    if (!config.contains("format")) {
        return LoadResult("No format specified in config");
    }
    
    std::string const format_id = config["format"];
    
    // Find appropriate loader
    auto& registry = LoaderRegistry::getInstance();
    DataLoader const* loader = registry.findLoader(format_id, data_type);
    
    if (!loader) {
        return LoadResult("No loader found for format '" + format_id + 
                         "' and data type " + std::to_string(static_cast<int>(data_type)));
    }
    
    // Load the data using the plugin
    return loader->loadData(file_path, data_type, config, factory);
}

bool PluginLoader::isFormatSupported(std::string const& format_id, IODataType data_type) {
    auto& registry = LoaderRegistry::getInstance();
    return registry.findLoader(format_id, data_type) != nullptr;
}

std::vector<std::string> PluginLoader::getSupportedFormats() {
    auto& registry = LoaderRegistry::getInstance();
    return registry.getRegisteredFormats();
}
