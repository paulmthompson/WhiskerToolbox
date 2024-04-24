#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "Media/Media_Data.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame.hpp"

#include <string>
#include <memory>
#include <unordered_map>

#if defined _WIN32 || defined __CYGWIN__
    #define DLLOPT __declspec(dllexport)
#else
    #define DLLOPT __attribute__((visibility("default")))
#endif

class DLLOPT DataManager {

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
    std::shared_ptr<LineData> getLine(const std::string line_key);

    std::shared_ptr<TimeFrame> getTime() {return _time;};

private:

    std::shared_ptr<MediaData> _media;
    std::unordered_map<std::string,std::shared_ptr<LineData>> _lines;

    std::shared_ptr<TimeFrame> _time;

};



#endif // DATAMANAGER_HPP
