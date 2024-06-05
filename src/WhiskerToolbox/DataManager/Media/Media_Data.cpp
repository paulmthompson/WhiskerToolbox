
#include "Media/Media_Data.hpp"

MediaData::MediaData() :
    _width{640},
    _height{480}
{
    _rawData = std::vector<uint8_t>(_height * _width);
    setFormat(DisplayFormat::Gray);
};

MediaData::~MediaData()
{

}

void MediaData::setFormat(DisplayFormat const format)
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
    _rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::updateHeight(int const height)
{
    _height = height;
    _rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::updateWidth(int const width)
{
    _width = width;
    _rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::LoadMedia(std::string const& name)
{
    doLoadMedia(name);
}

std::vector<uint8_t> const& MediaData::getRawData(int const frame_number)
{
    LoadFrame(frame_number);
    return _rawData;
}
