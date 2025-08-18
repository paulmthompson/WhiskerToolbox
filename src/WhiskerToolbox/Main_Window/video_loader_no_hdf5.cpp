#include "video_loader.hpp"
#include "DataManager.hpp"
#include "DataManager/Media/MediaDataFactory.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

MediaLoadResult loadMediaData(MediaData::MediaType media_type, const std::string& file_path) {
    MediaLoadResult result;
    result.success = false;
    
    // Check if the requested media type is available
    if (!MediaDataFactory::isMediaTypeAvailable(media_type)) {
        result.error_message = "Requested media type is not available (feature not compiled in)";
        return result;
    }
    
    try {
        // Create the media data object using the factory
        auto media_data = MediaDataFactory::createMediaData(media_type);
        if (!media_data) {
            result.error_message = "Failed to create media data object";
            return result;
        }
        
        // Load the media file
        media_data->LoadMedia(file_path);
        
        result.media_data = media_data;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.error_message = "Exception during media loading: " + std::string(e.what());
    } catch (...) {
        result.error_message = "Unknown exception during media loading";
    }
    
    return result;
}

std::optional<MediaData::MediaType> detectMediaTypeFromExtension(const std::string& file_path) {
    auto vid_path = std::filesystem::path(file_path);
    auto extension = vid_path.extension().string();
    
    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".mp4" || extension == ".avi" || extension == ".mov") {
        return MediaData::MediaType::Video;
    } 
    else if (extension == ".h5" || extension == ".hdf5" || extension == ".mat") {
        return MediaData::MediaType::HDF5;
    }
    else if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || 
             extension == ".tiff" || extension == ".tif" || extension == ".bmp") {
        return MediaData::MediaType::Images;
    }
    
    return std::nullopt;
}

bool loadVideoData(const std::string& file_path, DataManager* data_manager) {
    // Auto-detect media type from file extension
    auto detected_type = detectMediaTypeFromExtension(file_path);
    if (!detected_type) {
        auto vid_path = std::filesystem::path(file_path);
        std::cout << "Video file with extension " << vid_path.extension() 
                  << " not supported" << std::endl;
        return false;
    }
    
    // Special handling for HDF5 when not available
    if (*detected_type == MediaData::MediaType::HDF5 && 
        !MediaDataFactory::isMediaTypeAvailable(MediaData::MediaType::HDF5)) {
        auto vid_path = std::filesystem::path(file_path);
        std::cout << "HDF5/MAT file support is not enabled in this build. "
                  << "Please rebuild with ENABLE_HDF5=ON to load " 
                  << vid_path.extension() << " files." << std::endl;
        return false;
    }
    
    // Try to load the media data
    auto result = loadMediaData(*detected_type, file_path);
    if (!result.success) {
        std::cout << "Failed to load media data: " << result.error_message << std::endl;
        return false;
    }
    
    // Set the loaded data in the DataManager
    data_manager->setData<MediaData>("media", result.media_data, TimeKey("time"));
    return true;
}
