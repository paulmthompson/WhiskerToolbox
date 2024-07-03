
#include "Media/Media_Data.hpp"

#include "utils/opencv_utility.hpp"

#include <opencv2/core/mat.hpp>

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

std::vector<uint8_t> MediaData::getProcessedData(const int frame_number)
{
    LoadFrame(frame_number);

    std::vector<uint8_t> output = _rawData;

    auto m2 = convert_vector_to_mat(output, getWidth(),getHeight());

    //cv::convertScaleAbs(m2, m2, 1.5, 0.0);
    //cv::medianBlur(m2,m2,15);

    for (auto const & [key, process] : _process_chain)
    {
        process(m2);
    }

    m2.reshape(1,getWidth()*getHeight());

    output.assign(m2.data, m2.data + m2.total() *m2.channels());

    return output;
}

void MediaData::setProcess(std::string key, std::function<void(cv::Mat& input)> process){
    this->_process_chain[key] = process;
}


