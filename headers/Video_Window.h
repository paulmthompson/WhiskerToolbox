#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include "Media_Window.h"

#include <memory>
#include <string>

class Video_Window : public Media_Window
{
 Q_OBJECT
public:
    Video_Window(QObject *parent = 0);

private:
    int doLoadMedia(std::string name) override;
    int doLoadFrame(int frame_id) override;
    int GetVideoInfo(std::string name);

};
#endif // VIDEO_WINDOW_H
