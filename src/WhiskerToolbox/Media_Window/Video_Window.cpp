
#include "Video_Window.h"
#include "Video_Data.hpp"

Video_Window::Video_Window(QObject *parent) : Media_Window(parent)
{

    this->media = std::make_shared<VideoData>();

    //this->vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();

    //last_decoded_frame = 0;
}

int Video_Window::FindNearestSnapFrame(int frame_id) const {
    return dynamic_cast<VideoData*>(this->media.get())->FindNearestSnapFrame(frame_id);
}