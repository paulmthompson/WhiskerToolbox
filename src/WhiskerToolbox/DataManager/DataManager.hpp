#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "Media/Media_Data.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <functional> // std::function
#include <memory> // std::shared_ptr
#include <string> // std::string
#include <unordered_map> // std::unordered_map
#include <utility> // std::move
#include <vector> // std::vector

class AnalogTimeSeries;
class TimeFrame;

class DataManager {

public:
    DataManager();
    enum MediaType {
        Video,
        Images,
    };

    void createMedia(MediaType);
    void loadMedia(std::string filepath);
    std::shared_ptr<MediaData> getMediaData();

    void createPoint(std::string const & point_key);
    std::shared_ptr<PointData> getPoint(std::string const & point_key);
    std::vector<std::string> getPointKeys();

    void createLine(const std::string line_key);
    std::shared_ptr<LineData> getLine(const std::string line_key);
    std::vector<std::string> getLineKeys();

    void createMask(const std::string& mask_key);
    void createMask(const std::string& mask_key, int const width, int const height);
    std::shared_ptr<MaskData> getMask(const std::string& mask_key);
    std::vector<std::string> getMaskKeys();

    void createAnalogTimeSeries(std::string const & analog_key);
    std::shared_ptr<AnalogTimeSeries> getAnalogTimeSeries(std::string const & analog_key);
    std::vector<std::string> getAnalogTimeSeriesKeys();

    std::shared_ptr<TimeFrame> getTime() {return _time;};

    using ObserverCallback = std::function<void()>;

    void addObserver(ObserverCallback callback) {
        _observers.push_back(std::move(callback));
    }

    // Method to notify all observers of a change
    void notifyObservers() {
        for (auto& observer : _observers) {
            observer(); // Call the observer callback
        }
    }

private:

    std::shared_ptr<MediaData> _media;

    std::unordered_map<std::string,std::shared_ptr<PointData>> _points;

    std::unordered_map<std::string,std::shared_ptr<LineData>> _lines;

    std::unordered_map<std::string, std::shared_ptr<MaskData>> _masks;

    std::unordered_map<std::string, std::shared_ptr<AnalogTimeSeries>> _analog;

    std::shared_ptr<TimeFrame> _time;

    std::vector<ObserverCallback> _observers;

};

std::vector<std::vector<float>> read_ragged_hdf5(std::string const & filepath, std::string const & key);
std::vector<int> read_array_hdf5(std::string const & filepath, std::string const & key);


#endif // DATAMANAGER_HPP
