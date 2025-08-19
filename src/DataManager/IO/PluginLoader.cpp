#include "PluginLoader.hpp"
#include "LoaderRegistry.hpp"

#include <algorithm>
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
    
    // Use the new registry system
    auto& registry = LoaderRegistry::getInstance();
    return registry.tryLoad(format_id, data_type, file_path, config, factory);
}

bool PluginLoader::isFormatSupported(std::string const& format_id, IODataType data_type) {
    auto& registry = LoaderRegistry::getInstance();
    return registry.isFormatSupported(format_id, data_type);
}

std::vector<std::string> PluginLoader::getSupportedFormats() {
    auto& registry = LoaderRegistry::getInstance();
    // For now, return formats for all data types - could be enhanced to filter by data type
    std::vector<std::string> all_formats;
    for (auto data_type : {IODataType::Line, IODataType::Points, IODataType::Mask}) {
        auto formats = registry.getSupportedFormats(data_type);
        for (auto const& format : formats) {
            if (std::find(all_formats.begin(), all_formats.end(), format) == all_formats.end()) {
                all_formats.push_back(format);
            }
        }
    }
    return all_formats;
}
