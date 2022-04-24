#include "whiskertracker.h"

#include <stdio.h>

Image<uint8_t> bg = Image<uint8_t>(640,480,std::vector<uint8_t>(640*480,0));

WhiskerTracker::WhiskerTracker()
{
    janelia = JaneliaTracker();
    janelia_init = false;
    whiskers = std::vector<Whisker>{};
}

void WhiskerTracker::trace(std::vector<uint8_t>& input) {

    if (this->janelia_init == false) {
        this->janelia.bank = LineDetector(this->janelia.config);
        this->janelia.half_space_bank = HalfSpaceDetector(this->janelia.config);
        this->janelia_init = true;
    }

    whiskers.clear();

    Image<uint8_t>img = Image<uint8_t>(640,480,input);
    std::vector<Whisker_Seg> j_segs = janelia.find_segments(1,img,bg);

    int whisker_count = 1;
    for (auto& w_seg : j_segs) {
        whiskers.push_back(Whisker(whisker_count++,std::move(w_seg.x),std::move(w_seg.y)));
    }
}
