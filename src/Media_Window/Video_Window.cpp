
#include "Video_Window.h"

Video_Window::Video_Window(QObject *parent) : Media_Window(parent) {
    vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
}

int Video_Window::doLoadMedia(std::string name) {
    this->vid_name = name;
    this->vd->createMedia(this->vid_name);

    this->mediaHeight = vd->getHeight();
    this->mediaWidth = vd->getWidth();

    this->mediaData.resize(this->mediaWidth * this->mediaHeight);

    return vd->getFrameCount(); // Total frames
}

void Video_Window::doLoadFrame(int frame_id) {

    //In most circumstances, we want to decode forward from the current frame without reseeking to a keyframe
    bool frame_by_frame = true;

    if (frame_id == 0) {
        frame_by_frame = false;
    } else if (frame_id >= this->total_frame_count - 1) {
        frame_by_frame = false;
    } else if (frame_id <= this->last_loaded_frame) {
        frame_by_frame = false;
    }

    //We load the data associated with the frame
    this->mediaData = vd->getFrame( frame_id, frame_by_frame);

    this->mediaImage = QImage(&this->mediaData[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
}

std::string Video_Window::doGetFrameID(int frame_id) {
    return std::to_string(frame_id);
}

int Video_Window::FindNearestSnapFrame(int frame_id) const {
    return this->vd->nearest_iframe(frame_id);
}
