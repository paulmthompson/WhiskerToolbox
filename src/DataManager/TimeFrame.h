#ifndef TIMEFRAME_H
#define TIMEFRAME_H


class TimeFrame {
public:
    void updateLastLoadedFrame(int frame) {this->last_loaded_frame = frame;};
    int getLastLoadedFrame() const {return this->last_loaded_frame;};
protected:
    int last_loaded_frame;
};


#endif // TIMEFRAME_H
