#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include "Media_Window.h"
#include "Media_Data.h"

#include <ffmpeg_wrapper/videodecoder.h>

#include <memory>
#include <string>

class Video_Window : public Media_Window
{
 Q_OBJECT
public:
    Video_Window(QObject *parent = 0);

    int FindNearestSnapFrame(int frame_id) const;

private:

};
#endif // VIDEO_WINDOW_H
