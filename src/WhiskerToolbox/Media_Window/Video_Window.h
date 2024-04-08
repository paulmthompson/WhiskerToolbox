#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include "Media_Window.h"

#include <ffmpeg_wrapper/videodecoder.h>

#include <memory>
#include <string>

class VideoData : public MediaData {
public:
    VideoData();
    int LoadMedia(std::string name) override;
    void LoadFrame(int frame_id) override;
    std::string GetFrameID(int frame_id) override;

    int FindNearestSnapFrame(int frame_id) const;
protected:

private:
    int _last_decoded_frame;
    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> _vd;
};

class Video_Window : public Media_Window
{
 Q_OBJECT
public:
    Video_Window(QObject *parent = 0);

    int FindNearestSnapFrame(int frame_id) const;

private:

};
#endif // VIDEO_WINDOW_H
