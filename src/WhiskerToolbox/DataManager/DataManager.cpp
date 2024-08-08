
#include "DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Media/HDF5_Data.hpp"
#include "Media/Image_Data.hpp"
#include "Media/Video_Data.hpp"
#include "TimeFrame.hpp"

#include "utils/hdf5_mask_load.hpp"
#include "utils/container.hpp"

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
        _media.reset();
        _media = std::make_shared<ImageData>();
        break;
    }
    case (MediaType::Video):
    {
        _media.reset();
        _media = std::make_shared<VideoData>();
        break;
    }
    case (MediaType::HDF5):
    {
        _media.reset();
        _media = std::make_shared<HDF5Data>();
        break;
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

void DataManager::createPoint(std::string const & point_key)
{
    _points[point_key] = std::make_shared<PointData>();
}

std::shared_ptr<PointData> DataManager::getPoint(std::string const & point_key)
{
    return _points[point_key];
}

std::vector<std::string> DataManager::getPointKeys()
{
    return get_keys(_points);
}

void DataManager::createLine(const std::string line_key)
{
    _lines[line_key] = std::make_shared<LineData>();
}

std::shared_ptr<LineData> DataManager::getLine(const std::string line_key)
{

    return _lines[line_key];
}

std::vector<std::string> DataManager::getLineKeys()
{
    return get_keys(_lines);
}

void DataManager::createMask(const std::string& mask_key)
{
    _masks[mask_key] = std::make_shared<MaskData>();
}

void DataManager::createMask(const std::string& mask_key, int const width, int const height)
{
    _masks[mask_key] = std::make_shared<MaskData>();
    _masks[mask_key]->setMaskWidth(width);
    _masks[mask_key]->setMaskHeight(height);
}

std::shared_ptr<MaskData> DataManager::getMask(const std::string& mask_key)
{

    return _masks[mask_key];
}

std::vector<std::string> DataManager::getMaskKeys()
{
    return get_keys(_masks);
}

void DataManager::createAnalogTimeSeries(std::string const & key)
{
    _analog[key] = std::make_shared<AnalogTimeSeries>();
}

std::shared_ptr<AnalogTimeSeries> DataManager::getAnalogTimeSeries(std::string const & analog_key)
{
    return _analog[analog_key];
}

std::vector<std::string> DataManager::getAnalogTimeSeriesKeys()
{
    return get_keys(_analog);
}

void DataManager::createDigitalTimeSeries(std::string const & digital_key)
{
    _digital[digital_key] = std::make_shared<DigitalTimeSeries>();
}

std::shared_ptr<DigitalTimeSeries> DataManager::getDigitalTimeSeries(std::string const & digital_key)
{
    return _digital[digital_key];
}

std::vector<std::string> DataManager::getDigitalTimeSeriesKeys()
{
    return get_keys(_digital);
}

std::vector<std::vector<float>> read_ragged_hdf5(std::string const & filepath, std::string const & key)
{
    auto myvector = load_ragged_array<float>(filepath, key);
    return myvector;
}

std::vector<int> read_array_hdf5(std::string const & filepath, std::string const & key)
{
    auto myvector = load_array<int>(filepath, key);
    return myvector;
}
