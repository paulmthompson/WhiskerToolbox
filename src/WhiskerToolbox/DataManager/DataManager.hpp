#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "Media/Media_Data.hpp"

#include <functional> // std::function
#include <memory> // std::shared_ptr
#include <string> // std::string
#include <unordered_map> // std::unordered_map
#include <utility> // std::move
#include <variant> // std::variant
#include <vector> // std::vector

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class PointData;
class LineData;
class MaskData;

class TimeFrame;

struct DataInfo {
    std::string key;
    std::string data_class;
    std::string color;
};

class DataManager {

public:
    DataManager();

    void setMedia(std::shared_ptr<MediaData> media) {
        _data["media"] = media;
    };

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

    void loadFromJSON(std::string const & filepath);

    void load_A(const std::string & filepath);
    void load_B(const std::string & filepath);

    std::vector<std::string> getAllKeys()
    {
        std::vector<std::string> keys;
        for (const auto& [key, value] : _data) {

            keys.push_back(key);

        }
        return keys;
    }

    template<typename T>
    std::vector<std::string> getKeys()
    {
        std::vector<std::string> keys;
        for (const auto& [key, value] : _data) {
            if (std::holds_alternative<std::shared_ptr<T>>(value)) {
                keys.push_back(key);
            }
        }
        return keys;
    }

    template<typename T>
    std::shared_ptr<T> getData(const std::string& key) {
        if (_data.find(key) != _data.end()) {
            return std::get<std::shared_ptr<T>>(_data[key]);
        }
        return nullptr;
    }

    template<typename T>
    void setData(const std::string& key){
        _data[key] = std::make_shared<T>();
    }

    template<typename T>
    void setData(const std::string& key, std::shared_ptr<T> data){
        _data[key] = data;
    }

    std::string getType(const std::string& key) const;

private:

    std::shared_ptr<TimeFrame> _time;

    std::vector<ObserverCallback> _observers;

    std::unordered_map<std::string, std::variant<
                                        std::shared_ptr<MediaData>,
                                        std::shared_ptr<PointData>,
                                        std::shared_ptr<LineData>,
                                        std::shared_ptr<MaskData>,
                                        std::shared_ptr<AnalogTimeSeries>,
                                        std::shared_ptr<DigitalEventSeries>,
                                        std::shared_ptr<DigitalIntervalSeries>>> _data;

};

std::vector<std::vector<float>> read_ragged_hdf5(std::string const & filepath, std::string const & key);

std::vector<int> read_array_hdf5(std::string const & filepath, std::string const & key);

std::vector<DataInfo> load_data_from_json_config(std::shared_ptr<DataManager>, std::string json_filepath);


#endif // DATAMANAGER_HPP
