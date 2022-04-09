#include "whiskertracker.h"

struct WhiskerTracker::Whisker {
    int id; // frame
    std::vector<float> x;
    std::vector<float> y;
};


WhiskerTracker::WhiskerTracker()
{

}

