#ifndef WHISKERTOOLBOX_VIDEO_DATA_LOADER_HPP
#define WHISKERTOOLBOX_VIDEO_DATA_LOADER_HPP

#include "Media/Video_Data.hpp"

#include <iostream>
#include <memory>
#include <string>

inline std::shared_ptr<VideoData> load_video_into_VideoData(std::string const & file_path) {

    auto video_data = std::make_shared<VideoData>();

    video_data->LoadMedia(file_path);

    std::cout << "Video has " << video_data->getTotalFrameCount() << " frames" << std::endl;

    return video_data;
}

#endif//WHISKERTOOLBOX_VIDEO_DATA_LOADER_HPP
