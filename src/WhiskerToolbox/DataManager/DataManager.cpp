
#include "DataManager.h"

#include "Media/Video_Data.hpp"
#include "Media/Image_Data.h"


DataManager::DataManager() :
    _media{std::make_shared<MediaData>()}
{

}

void DataManager::createMedia(MediaType media_type)
{
    switch (media_type)
    {
    case (MediaType::Images):
    {
        _media.reset(new ImageData{});
    }
    case (MediaType::Video):
    {
        _media.reset();
        _media = std::make_shared<VideoData>();
    }
    }
}

void DataManager::loadMedia(std::string filepath)
{

}

std::shared_ptr<MediaData> DataManager::getMediaData()
{
    return _media;
}
