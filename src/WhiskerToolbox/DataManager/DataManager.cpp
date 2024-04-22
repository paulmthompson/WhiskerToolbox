
#include "DataManager.hpp"

#include "Media/Video_Data.hpp"
#include "Media/Image_Data.hpp"


DataManager::DataManager() :
    _media{std::make_shared<MediaData>()},
    _time{std::make_shared<TimeFrame>()}
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

void DataManager::createLine(const std::string line_key)
{
    _lines[line_key] = std::make_shared<LineData>();
}

std::shared_ptr<LineData> DataManager::getLine(const std::string line_key)
{

    return _lines[line_key];
}
