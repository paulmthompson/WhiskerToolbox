#ifndef WHISKERTOOLBOX_MEDIA_DATA_FACTORY_HPP
#define WHISKERTOOLBOX_MEDIA_DATA_FACTORY_HPP

#include "DataManagerFwd.hpp"
#include "Media/Media_Data.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Factory for creating MediaData subclasses
 * 
 * This factory uses runtime registration to create MediaData objects
 * based on compile-time availability. Media types register themselves
 * during static initialization based on CMake flags.
 */
class MediaDataFactory {
public:
    using MediaCreatorFunc = std::function<std::shared_ptr<MediaData>()>;
    using MediaLoaderFunc = std::function<std::shared_ptr<MediaData>(std::string const &, nlohmann::json const &)>;

    /**
     * @brief Register a media type creator function
     * @param media_type The MediaData::MediaType to register
     * @param creator Function that creates an instance of the media type
     */
    static void registerMediaType(MediaData::MediaType media_type, MediaCreatorFunc creator);

    /**
     * @brief Register a media type loader function
     * @param dm_type The DM_DataType to register
     * @param loader Function that loads data and returns a MediaData instance
     */
    static void registerMediaLoader(DM_DataType dm_type, MediaLoaderFunc loader);

    /**
     * @brief Create a MediaData instance of the specified type
     * @param media_type The type of MediaData to create
     * @return Shared pointer to the created MediaData, or nullptr if type not registered
     */
    static std::shared_ptr<MediaData> createMediaData(MediaData::MediaType media_type);

    /**
     * @brief Load media data from file
     * @param dm_type The DM_DataType indicating what to load
     * @param file_path Path to the media file
     * @param config JSON configuration for loading
     * @return Shared pointer to the loaded MediaData, or nullptr if type not registered
     */
    static std::shared_ptr<MediaData> loadMediaData(DM_DataType dm_type, std::string const & file_path, nlohmann::json const & config);

    /**
     * @brief Check if a media type is registered/available
     * @param media_type The MediaType to check
     * @return true if the type is registered and available
     */
    static bool isMediaTypeAvailable(MediaData::MediaType media_type);

    /**
     * @brief Check if a DM_DataType loader is registered/available
     * @param dm_type The DM_DataType to check
     * @return true if the type has a registered loader
     */
    static bool isLoaderAvailable(DM_DataType dm_type);

    /**
     * @brief Get all registered media types
     * @return Vector of all registered MediaTypes
     */
    static std::vector<MediaData::MediaType> getRegisteredMediaTypes();

    /**
     * @brief Get all registered DM_DataTypes with loaders
     * @return Vector of all DM_DataTypes with registered loaders
     */
    static std::vector<DM_DataType> getRegisteredLoaderTypes();

private:
    static std::map<MediaData::MediaType, MediaCreatorFunc> & getCreatorRegistry();
    static std::map<DM_DataType, MediaLoaderFunc> & getLoaderRegistry();
};

/**
 * @brief Helper class for registering media types during static initialization
 */
template<MediaData::MediaType Type>
class MediaTypeRegistrar {
public:
    MediaTypeRegistrar(MediaDataFactory::MediaCreatorFunc creator) {
        MediaDataFactory::registerMediaType(Type, std::move(creator));
    }
};

/**
 * @brief Helper class for registering media loaders during static initialization
 */
template<DM_DataType Type>
class MediaLoaderRegistrar {
public:
    MediaLoaderRegistrar(MediaDataFactory::MediaLoaderFunc loader) {
        MediaDataFactory::registerMediaLoader(Type, std::move(loader));
    }
};

// Convenience macros for registration
#define REGISTER_MEDIA_TYPE(type, creator_func) \
    static MediaTypeRegistrar<MediaData::MediaType::type> type##_registrar(creator_func);

#define REGISTER_MEDIA_LOADER(dm_type, loader_func) \
    static MediaLoaderRegistrar<DM_DataType::dm_type> dm_type##_loader_registrar(loader_func);

#endif// WHISKERTOOLBOX_MEDIA_DATA_FACTORY_HPP
