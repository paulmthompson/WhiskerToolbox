#include "Images_Window.h"
#include "Image_Data.h"

#include <iostream>
#include <set>

namespace fs = std::filesystem;


Images_Window::Images_Window(QObject *parent) : Media_Window(parent) {
    setData(std::make_shared<ImageData>());
}
