
#include "Media/Media_Data.hpp"

MediaData::MediaData() :
    _width{640},
    _height{480}
{
    rawData = std::vector<uint8_t>(_height * _width);
    setFormat(DisplayFormat::Gray);
};

void MediaData::setFormat(DisplayFormat format)
{
    _format = format;
    switch(_format)
    {
    case DisplayFormat::Gray:
        _display_format_bytes = 1;
        break;
    case DisplayFormat::Color:
        _display_format_bytes = 4;
        break;
    default:
        _display_format_bytes = 1;
        break;
    }
    this->rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::updateHeight(int height)
{
    _height = height;
    this->rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::updateWidth(int width)
{
    _width = width;
    this->rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::LoadMedia(std::string name)
{
    doLoadMedia(name);
}
