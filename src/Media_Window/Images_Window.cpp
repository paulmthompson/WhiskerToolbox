#include "Images_Window.h"

#include <iostream>

namespace fs = std::filesystem;

Images_Window::Images_Window(QObject *parent) : Media_Window(parent) {

}

int Images_Window::doLoadMedia(std::string dir_name) {

    auto file_extension = ".png";

    for (const auto & entry : fs::directory_iterator(dir_name)) {
        if (entry.path().extension() == file_extension) {
            this->image_paths.push_back(dir_name / entry.path());
        }
    }

    return this->image_paths.size();
}

int Images_Window::doLoadFrame(int frame_id) {

    this->myimage = QImage(QString::fromStdString(this->image_paths[frame_id].string()));

    this->current_frame = std::vector<uint8_t>(this->myimage.bits(), this->myimage.bits() + this->myimage.sizeInBytes());

    return frame_id;
}

int Images_Window::doFindNearestSnapFrame(int frame_id) const {

    return frame_id;
}
