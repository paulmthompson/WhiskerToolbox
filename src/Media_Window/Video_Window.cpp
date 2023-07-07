
#include "Video_Window.h"

Video_Window::Video_Window(QObject *parent) : Media_Window(parent) {

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
