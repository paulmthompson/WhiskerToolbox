#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "Media/Media_Data.hpp"
#include "Lines/Line_Data.hpp"

#include <string>
#include <memory>
#include <unordered_map>

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

    void createLine(const std::string line_key);

private:

    std::shared_ptr<MediaData> _media;
    std::unordered_map<std::string,std::unique_ptr<LineData>> _lines;
};



#endif // DATAMANAGER_HPP
