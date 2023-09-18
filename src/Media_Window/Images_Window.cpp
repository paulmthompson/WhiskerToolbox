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

void Images_Window::doLoadFrame(int frame_id) {

    this->canvasImage = QImage(QString::fromStdString(this->image_paths[frame_id].string()));

    //Note that this is not necessary a uint8_t because the format of the QImage above is automatically assigned and may not be Gray8.
    this->current_frame = std::vector<uint8_t>(this->canvasImage.bits(), this->canvasImage.bits() + this->canvasImage.sizeInBytes());
}

std::string Images_Window::doGetFrameID(int frame_id) {
    return this->image_paths[frame_id].filename().string();
}

std::pair<int,int> Images_Window::doGetMediaDimensions() const {
    return std::pair<int,int>{this->canvasImage.height(),this->canvasImage.width()};
}
