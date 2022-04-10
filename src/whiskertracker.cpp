#include "whiskertracker.h"

Image<uint8_t> bg = Image<uint8_t>(640,480,std::vector<uint8_t>(640*480,0));

WhiskerTracker::WhiskerTracker()
{
    janelia = JaneliaTracker();
}

std::vector<Whisker> WhiskerTracker::trace(std::vector<uint8_t> input) {

    Image<uint8_t> *img = nullptr;
    *img = Image<uint8_t>(640,480,input);
    std::vector<Whisker_Seg> j_segs = janelia.find_segments(1,img,bg);

    std::vector<Whisker> output = {};
    //for (auto w_seg : j_segs) {
    //    output.push_back(Whisker(1,w_seg.x,w_seg.y));
    //}

    return output;
}
