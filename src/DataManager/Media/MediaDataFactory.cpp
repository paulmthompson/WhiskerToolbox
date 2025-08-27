#include "Media/MediaDataFactory.hpp"
#include <iostream>

std::map<MediaData::MediaType, MediaDataFactory::MediaCreatorFunc>& MediaDataFactory::getCreatorRegistry() {
    static std::map<MediaData::MediaType, MediaCreatorFunc> registry;
    return registry;
}

std::map<DM_DataType, MediaDataFactory::MediaLoaderFunc>& MediaDataFactory::getLoaderRegistry() {
    static std::map<DM_DataType, MediaLoaderFunc> registry;
    return registry;
}

void MediaDataFactory::registerMediaType(MediaData::MediaType media_type, MediaCreatorFunc creator) {
    auto& registry = getCreatorRegistry();
    registry[media_type] = std::move(creator);
    //std::cout << "MediaDataFactory: Registered media type " << static_cast<int>(media_type) << std::endl;
}

void MediaDataFactory::registerMediaLoader(DM_DataType dm_type, MediaLoaderFunc loader) {
    auto& registry = getLoaderRegistry();
    registry[dm_type] = std::move(loader);
   // std::cout << "MediaDataFactory: Registered loader for DM_DataType " << static_cast<int>(dm_type) << std::endl;
}

std::shared_ptr<MediaData> MediaDataFactory::createMediaData(MediaData::MediaType media_type) {
    auto& registry = getCreatorRegistry();
    auto it = registry.find(media_type);
    if (it != registry.end()) {
        return it->second();
    }
    std::cerr << "MediaDataFactory: No creator registered for media type " << static_cast<int>(media_type) << std::endl;
    return nullptr;
}

std::shared_ptr<MediaData> MediaDataFactory::loadMediaData(DM_DataType dm_type, std::string const& file_path, nlohmann::json const& config) {
    auto& registry = getLoaderRegistry();
    auto it = registry.find(dm_type);
    if (it != registry.end()) {
        return it->second(file_path, config);
    }
    std::cerr << "MediaDataFactory: No loader registered for DM_DataType " << static_cast<int>(dm_type) << std::endl;
    return nullptr;
}

bool MediaDataFactory::isMediaTypeAvailable(MediaData::MediaType media_type) {
    auto& registry = getCreatorRegistry();
    return registry.find(media_type) != registry.end();
}

bool MediaDataFactory::isLoaderAvailable(DM_DataType dm_type) {
    auto& registry = getLoaderRegistry();
    return registry.find(dm_type) != registry.end();
}

std::vector<MediaData::MediaType> MediaDataFactory::getRegisteredMediaTypes() {
    auto& registry = getCreatorRegistry();
    std::vector<MediaData::MediaType> types;
    types.reserve(registry.size());
    for (auto const& [type, creator] : registry) {
        types.push_back(type);
    }
    return types;
}

std::vector<DM_DataType> MediaDataFactory::getRegisteredLoaderTypes() {
    auto& registry = getLoaderRegistry();
    std::vector<DM_DataType> types;
    types.reserve(registry.size());
    for (auto const& [type, loader] : registry) {
        types.push_back(type);
    }
    return types;
}
