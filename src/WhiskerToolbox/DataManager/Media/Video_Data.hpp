#ifndef WHISKERTOOLBOX_VIDEO_DATA_HPP
#define WHISKERTOOLBOX_VIDEO_DATA_HPP

#include "Media/Media_Data.hpp"

#include <memory>
#include <string>

// I can forward declare VideoDecoder as a unique_ptr member variable
// But i need a destructor declaration in header and empty definition
// in the cpp file for it to be a complete type
//https://stackoverflow.com/questions/6012157/is-stdunique-ptrt-required-to-know-the-full-definition-of-t
namespace ffmpeg_wrapper {
class VideoDecoder;
}

class VideoData : public MediaData {
public:
    VideoData();

    ~VideoData() override;

    std::string GetFrameID(int frame_id) const override;

    /**
     * When scrolling through large video files, it makes for much
     * smoother scrolling for the slider to "snap" or seek only
     * to key frames.
     *
     * @param frame_id
     * @return Nearest keyframe to frame_id
     */
    [[nodiscard]] int FindNearestSnapFrame(int frame_id) const;

    int getFrameIndexFromNumber(int frame_id) override { return frame_id; };

protected:
    void doLoadMedia(std::string const & name) override;
    void doLoadFrame(int frame_id) override;

private:
    int _last_decoded_frame{0};
    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> _vd;
};


#endif//WHISKERTOOLBOX_VIDEO_DATA_HPP
