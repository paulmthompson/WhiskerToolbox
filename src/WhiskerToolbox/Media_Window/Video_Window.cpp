
#include "Video_Window.h"
#include "Video_Data.hpp"

#include <memory>

Video_Window::Video_Window(QObject *parent) : Media_Window(parent)
{
//    setData(std::make_shared<VideoData>());
}

int Video_Window::FindNearestSnapFrame(int frame_id) const {
    return dynamic_cast<VideoData*>(getData().get())->FindNearestSnapFrame(frame_id);
}
