
#include "DataManager.h"
#include "Video_Data.hpp"
#include "Image_Data.h"


void DataManager::createMedia(MediaType media_type)
{
    switch (media_type)
    {
    case (MediaType::Images):
    {
        _media = std::make_shared<ImageData>();
    }
    case (MediaType::Video):
    {
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
