#include "video_loader.hpp"
#include "DataManager.hpp"
#include "DataManager/Media/Video_Data.hpp"
#include "DataManager/Media/HDF5_Data.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include <filesystem>
#include <iostream>

bool loadVideoData(const std::string& file_path, DataManager* data_manager) {
    auto vid_path = std::filesystem::path(file_path);
    auto extension = vid_path.extension();
    
    if (extension == ".mp4") {
        auto media = std::make_shared<VideoData>();
        media->LoadMedia(file_path);
        data_manager->setData<VideoData>("media", media, TimeKey("time"));
        return true;
    } 
    else if (extension == ".h5" || extension == ".mat") {
        auto media = std::make_shared<HDF5Data>();
        media->LoadMedia(file_path);
        data_manager->setData<HDF5Data>("media", media, TimeKey("time"));
        return true;
    } 
    else {
        std::cout << "Video file with extension " << extension 
                  << " not supported" << std::endl;
        return false;
    }
}
