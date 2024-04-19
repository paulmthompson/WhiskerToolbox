#include "Images_Window.h"
#include "Image_Data.h"


Images_Window::Images_Window(QObject *parent) : Media_Window(parent) {
    setData(std::make_shared<ImageData>());
}
