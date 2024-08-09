
#include "Media/Image_Data.hpp"

#include "utils/string_manip.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/core.hpp>

#include <cstddef>
#include <iostream>
#include <set>


ImageData::ImageData() = default;

void ImageData::doLoadMedia(std::string dir_name) {

    auto file_extensions = std::set<std::string>{".png",".jpg"};

    for (const auto & entry : std::filesystem::directory_iterator(dir_name)) {
        if (file_extensions.count(entry.path().extension().string())) {
            _image_paths.push_back(dir_name / entry.path());
        }
    }

    if (_image_paths.size() == 0) {
        std::cout << "Warning: No images found in directory with matching extensions ";
        for (auto const &i : file_extensions) {
            std::cout << i << " ";
        }
        std::cout << std::endl;
    }

    setTotalFrameCount(_image_paths.size());
}

cv::Mat _convertToDisplayFormat(cv::Mat& image, ImageData::DisplayFormat format){
    cv::Mat converted_image;
    if (format == ImageData::DisplayFormat::Gray){
        cv::cvtColor(image, converted_image, cv::COLOR_BGR2GRAY);
        // std::cout << "Converting to Gray" << std::endl;
    } else if (format == ImageData::DisplayFormat::Color){
        cv::cvtColor(image, converted_image, cv::COLOR_BGR2BGRA);
        // std::cout << "Converting to 4channel" << std::endl;
    }
    return converted_image;
}

void ImageData::doLoadFrame(int frame_id) {

    if (frame_id > _image_paths.size()) {
        std::cout << "Error: Requested frame ID is larger than the number of frames in Media Data" << std::endl;
        return;
    }

    auto loaded_image = cv::imread(_image_paths[frame_id].string());

    updateHeight(loaded_image.rows);
    updateWidth(loaded_image.cols);

    auto converted_image = _convertToDisplayFormat(loaded_image, this->getFormat());
    
    size_t num_bytes = converted_image.total() * converted_image.elemSize();
    // std::cout << converted_image.elemSize() << ' ' << converted_image.total() << std::endl;
    this->setRawData(std::vector<uint8_t>(static_cast<uint8_t*>(converted_image.data), static_cast<uint8_t*>(converted_image.data) + num_bytes));
}

std::string ImageData::GetFrameID(int frame_id) {
    return _image_paths[frame_id].filename().string();
}

int ImageData::getFrameIndexFromNumber(int frame_id)
{
    for (std::size_t i = 0; i < _image_paths.size(); i++) {
        auto image_frame_id = extract_numbers_from_string(_image_paths[i].filename().string());
        if (std::stoi(image_frame_id) == frame_id) {
            return i;
        }
    }
    std::cout << "No matching frame found for requested ID" << std::endl;
    return 0;
}
