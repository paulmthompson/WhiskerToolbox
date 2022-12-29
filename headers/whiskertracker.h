#ifndef WHISKERTRACKER_H
#define WHISKERTRACKER_H

#include <unordered_map>
#include <vector>
#include "janelia.h"

struct Whisker {
    int id; // unique identifier for whisker in frame, starting at 1
    std::vector<float> x;
    std::vector<float> y;
    Whisker()
    {
        id = 0;
        x = std::vector<float>();
        y = std::vector<float>();
    }
    Whisker(int idd, std::vector<float> xx, std::vector<float> yy) {
        id=idd;
        x=xx;
        y=yy;
    }
};

class WhiskerTracker {

public:
    WhiskerTracker();

   void trace(const std::vector<uint8_t>& input);
   std::tuple<float,int> get_nearest_whisker(float x_p, float y_p);
    std::vector<Whisker> whiskers;


private:
    JaneliaTracker janelia;
    bool janelia_init;
    //std::unordered_map<int, Whisker> woi;

};




#endif // WHISKERTRACKER_H
