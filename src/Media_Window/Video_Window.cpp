
#include "Video_Window.h"

Video_Window::Video_Window(QObject *parent) : Media_Window(parent) {
    vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
}

int Video_Window::doLoadMedia(std::string name) {
    return this->GetVideoInfo(name);
}

int Video_Window::GetVideoInfo(std::string name)
{
    this->vid_name = name;
    this->vd->createMedia(this->vid_name);

    this->current_frame.resize(vd->getHeight()*vd->getWidth());

    return vd->getFrameCount(); // Total frames
}

int Video_Window::doLoadFrame(int frame_id) {

    //In most circumstances, we want to decode forward from the current frame without reseeking to a keyframe
    bool frame_by_frame = true;

    if (frame_id == 0) {
        frame_by_frame = false;
    } else if (frame_id >= this->total_frame_count - 1) {
        frame_by_frame = false;
    } else if (frame_id <= this->last_loaded_frame) {
        frame_by_frame = false;
    }

    this->current_frame = vd->getFrame( frame_id, frame_by_frame);

    if (this->verbose_frame) {
        std::cout << "Getting frame " << std::to_string(frame_id) << std::endl;
        std::cout << "Loaded frame " << frame_id << std::endl;
    }

    this->myimage = QImage(&this->current_frame[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);

    return frame_id;
}

int Video_Window::doFindNearestSnapFrame(int frame_id) const {
    return this->vd->nearest_iframe(frame_id);
}

std::string Video_Window::doGetFrameID(int frame_id) {
    return std::to_string(frame_id);
}

std::pair<int,int> Video_Window::doGetMediaDimensions() const {
    return std::pair<int,int>{vd->getHeight(),vd->getWidth()};
}

