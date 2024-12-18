#ifndef TIMEFRAME_HPP
#define TIMEFRAME_HPP

#include <vector>

class TimeFrame {
public:
    TimeFrame() = default;
    TimeFrame(std::vector<int> times);
    void updateTotalFrameCount(int frame_count) {_total_frame_count = frame_count;};
    int getTotalFrameCount() const {return _total_frame_count;};

    void updateLastLoadedFrame(int frame) {_last_loaded_frame = frame;};
    int getLastLoadedFrame() const {return _last_loaded_frame;};

    int checkFrameInbounds(int frame_id) {

        if (frame_id < 0) {
            frame_id = 0;
        } else if (frame_id >= _total_frame_count) {
            frame_id = _total_frame_count;
        }
        return frame_id;
    }
protected:

private:
    std::vector<int> _times;
    int _last_loaded_frame;
    int _total_frame_count;
};


#endif // TIMEFRAME_HPP
