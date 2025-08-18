#ifndef VIDEO_LOADER_CONDITIONAL_HPP
#define VIDEO_LOADER_CONDITIONAL_HPP

#include <memory>
#include <string>
#include <iostream>
#include <optional>
#include "DataManager/Media/Media_Data.hpp"

// Forward declarations
class DataManager;

/**
 * @brief Result structure for media loading operations
 */
struct MediaLoadResult {
    std::shared_ptr<MediaData> media_data;
    bool success;
    std::string error_message;
};

/**
 * @brief Load media data using the factory system
 * @param media_type The type of media to create and load
 * @param file_path Path to the media file
 * @return MediaLoadResult containing the loaded data or error information
 */
MediaLoadResult loadMediaData(MediaData::MediaType media_type, const std::string& file_path);

/**
 * @brief Detect media type from file extension
 * @param file_path Path to the media file
 * @return Optional MediaType if extension is recognized, nullopt otherwise
 */
std::optional<MediaData::MediaType> detectMediaTypeFromExtension(const std::string& file_path);

/**
 * @brief Load video data based on file extension and available features (legacy function)
 * @param file_path Path to the video file
 * @param data_manager Pointer to the DataManager instance
 * @return true if successful, false otherwise
 * @deprecated Use loadMediaData with explicit MediaType instead
 */
bool loadVideoData(const std::string& file_path, DataManager* data_manager);

#endif // VIDEO_LOADER_CONDITIONAL_HPP
