//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_IMAGE_DATA_H
#define WHISKERTOOLBOX_IMAGE_DATA_H

#include <filesystem>
#include <vector>
#include <string>
#include "Media_Data.h"
#include "Media_Window.h"

class ImageData : public MediaData {
public:
    ImageData();
    int LoadMedia(std::string name) override;
    void LoadFrame(int frame_id) override;
    std::string GetFrameID(int frame_id) override;

protected:

private:
    std::vector<std::filesystem::path> _image_paths;
};


#endif //WHISKERTOOLBOX_IMAGE_DATA_H
