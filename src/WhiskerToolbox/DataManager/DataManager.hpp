#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "Media/Media_Data.hpp"

#include <string>
#include <memory>

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
private:

    std::shared_ptr<MediaData> _media;

};

//Load data object
// If data object has new new time series, load it
// If new time series, update scrollbar?



#endif // DATAMANAGER_HPP
