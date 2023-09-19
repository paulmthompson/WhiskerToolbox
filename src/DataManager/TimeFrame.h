#ifndef TIMEFRAME_H
#define TIMEFRAME_H


class TimeFrame {
public:
    void updateTotalFrameCount(int frame_count) {this->total_frame_count = frame_count;};
    int getTotalFrameCount() const {return this->total_frame_count;};

    void updateLastLoadedFrame(int frame) {this->last_loaded_frame = frame;};
    int getLastLoadedFrame() const {return this->last_loaded_frame;};

    int checkFrameInbounds(int frame_id) {

        if (frame_id < 0) {
            frame_id = 0;
        } else if (frame_id >= this->total_frame_count - 1) {
            frame_id = this->total_frame_count - 1;
        }
        return frame_id;
    }
protected:
    int last_loaded_frame;
    int total_frame_count;
};


#endif // TIMEFRAME_H
