//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_IMAGE_DATA_HPP
#define WHISKERTOOLBOX_IMAGE_DATA_HPP

#include <filesystem>
#include <vector>
#include <string>
#include "Media/Media_Data.hpp"

class ImageData : public MediaData {
public:
    ImageData();
    void LoadFrame(int frame_id) override;
    std::string GetFrameID(int frame_id) override;

    int getFrameIndexFromNumber(int frame_id) override;
protected:
    void doLoadMedia(std::string name) override;
private:
    std::vector<std::filesystem::path> _image_paths;
};


#endif //WHISKERTOOLBOX_IMAGE_DATA_HPP
