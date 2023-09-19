#include "Images_Window.h"

#include <iostream>
#include <set>

namespace fs = std::filesystem;

Images_Window::Images_Window(QObject *parent) : Media_Window(parent) {

}

int Images_Window::doLoadMedia(std::string dir_name) {

    auto file_extensions = std::set<std::string>{".png",".jpg"};

    for (const auto & entry : fs::directory_iterator(dir_name)) {
        if (file_extensions.count(entry.path().extension().string())) {
            this->image_paths.push_back(dir_name / entry.path());
        }
    }

    if (this->image_paths.size() == 0) {
        std::cout << "Warning: No images found in directory with matching extensions ";
        for (auto const &i : file_extensions) {
            std::cout << i << " ";
        }
        std::cout << std::endl;
    }

    return this->image_paths.size();
}

void Images_Window::doLoadFrame(int frame_id) {

    this->mediaImage = QImage(QString::fromStdString(this->image_paths[frame_id].string()));

    this->media->updateHeight(this->mediaImage.height());
    this->media->updateWidth(this->mediaImage.width());

    //Note that this is not necessary a uint8_t because the format of the QImage above is automatically assigned and may not be Gray8.
    this->mediaData = std::vector<uint8_t>(this->mediaImage.bits(), this->mediaImage.bits() + this->mediaImage.sizeInBytes());
}

std::string Images_Window::doGetFrameID(int frame_id) {
    return this->image_paths[frame_id].filename().string();
}
