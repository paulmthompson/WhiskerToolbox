//
// Created by wanglab on 4/10/2024.
//

#include "Image_Data.h"

#include <set>
#include <iostream>

ImageData::ImageData() {

}

int ImageData::LoadMedia(std::string dir_name) {

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

    return _image_paths.size();
}

void ImageData::LoadFrame(int frame_id) {

    auto loaded_image = QImage(QString::fromStdString(_image_paths[frame_id].string()));

    updateHeight(loaded_image.height());
    updateWidth(loaded_image.width());

    this->setFormat(loaded_image.format());

    this->data = std::vector<uint8_t>(loaded_image.bits(), loaded_image.bits() + loaded_image.sizeInBytes());
}

std::string ImageData::GetFrameID(int frame_id) {
    return _image_paths[frame_id].filename().string();
}
