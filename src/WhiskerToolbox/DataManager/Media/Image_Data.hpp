
#ifndef WHISKERTOOLBOX_IMAGE_DATA_HPP
#define WHISKERTOOLBOX_IMAGE_DATA_HPP

#include "Media/Media_Data.hpp"

#include <filesystem>
#include <string>
#include <vector>


class ImageData : public MediaData {
public:
    ImageData();
    [[nodiscard]] std::string GetFrameID(int frame_id) const override;

    int getFrameIndexFromNumber(int frame_id) override;

    /**
     * @brief Set the image paths directly
     * 
     * This method allows setting the image paths directly instead of loading from a directory.
     * This is useful for the new loading pattern with options.
     * 
     * @param image_paths Vector of file paths to the image files
     */
    void setImagePaths(std::vector<std::filesystem::path> const & image_paths);

protected:
    void doLoadMedia(std::string const & name) override;
    void doLoadFrame(int frame_id) override;

private:
    std::vector<std::filesystem::path> _image_paths;
};


#endif//WHISKERTOOLBOX_IMAGE_DATA_HPP
