
#ifndef WHISKERTOOLBOX_IMAGE_DATA_HPP
#define WHISKERTOOLBOX_IMAGE_DATA_HPP

#include "Media/Media_Data.hpp"

#include <filesystem>
#include <string>
#include <vector>


class ImageData : public MediaData {
public:
    ImageData();
    std::string GetFrameID(int frame_id) override;

    int getFrameIndexFromNumber(int frame_id) override;

protected:
    void doLoadMedia(std::string const & name) override;
    void doLoadFrame(int frame_id) override;

private:
    std::vector<std::filesystem::path> _image_paths;
};


#endif//WHISKERTOOLBOX_IMAGE_DATA_HPP
