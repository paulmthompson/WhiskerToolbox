#ifndef VIDEO_LOADER_CONDITIONAL_HPP
#define VIDEO_LOADER_CONDITIONAL_HPP

#include <memory>
#include <string>
#include <iostream>

// Forward declarations
class DataManager;
class MediaData;

/**
 * @brief Load video data based on file extension and available features
 * @param file_path Path to the video file
 * @param data_manager Pointer to the DataManager instance
 * @return true if successful, false otherwise
 */
bool loadVideoData(const std::string& file_path, DataManager* data_manager);

#endif // VIDEO_LOADER_CONDITIONAL_HPP
