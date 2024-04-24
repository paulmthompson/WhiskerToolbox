//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_IMAGE_DATA_HPP
#define WHISKERTOOLBOX_IMAGE_DATA_HPP

#include <filesystem>
#include <vector>
#include <string>
#include "Media/Media_Data.hpp"

#if defined _WIN32 || defined __CYGWIN__
    #define DLLOPT __declspec(dllexport)
#else
    #define DLLOPT __attribute__((visibility("default")))
#endif

class DLLOPT ImageData : public MediaData {
public:
    ImageData();
    void LoadFrame(int frame_id) override;
    std::string GetFrameID(int frame_id) override;

protected:
    void doLoadMedia(std::string name) override;
private:
    std::vector<std::filesystem::path> _image_paths;
};


#endif //WHISKERTOOLBOX_IMAGE_DATA_HPP
