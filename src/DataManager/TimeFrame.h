#ifndef TIMEFRAME_H
#define TIMEFRAME_H


class TimeFrame {
public:
    void updateTotalFrameCount(int frame_count) {_total_frame_count = frame_count;};
    int getTotalFrameCount() const {return _total_frame_count;};

    void updateLastLoadedFrame(int frame) {_last_loaded_frame = frame;};
    int getLastLoadedFrame() const {return _last_loaded_frame;};

    int checkFrameInbounds(int frame_id) {

        if (frame_id < 0) {
            frame_id = 0;
        } else if (frame_id >= _total_frame_count - 1) {
            frame_id = _total_frame_count - 1;
        }
        return frame_id;
    }
protected:

private:
    int _last_loaded_frame;
    int _total_frame_count;
};


#endif // TIMEFRAME_H
