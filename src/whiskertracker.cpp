#include "whiskertracker.h"

#include <stdio.h>

Image<uint8_t> bg = Image<uint8_t>(640,480,std::vector<uint8_t>(640*480,0));

WhiskerTracker::WhiskerTracker()
{
    janelia = JaneliaTracker();
    whiskers = std::vector<Whisker>{};
}

void WhiskerTracker::trace(std::vector<uint8_t> input) {

    whiskers.clear();

    Image<uint8_t>img = Image<uint8_t>(640,480,input);
    std::vector<Whisker_Seg> j_segs = janelia.find_segments(1,img,bg);

    for (auto& w_seg : j_segs) {
        whiskers.push_back(Whisker(1,w_seg.x,w_seg.y));
    }
    printf("The number of whiskers detected is %d",whiskers.size());
    fflush(stdout);
}
