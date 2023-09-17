
#include "DataManager.h"


std::shared_ptr<VideoSeries> DataManager::loadVideo(std::string video_name) {

    auto video = std::make_shared<VideoSeries>();

    video->LoadMedia(video_name);

    // Is this okay or am I splicing here?
    data.push_back(video);

    return video;
}
