#include "whiskertracker.h"

#include <cmath>
#include <stdio.h>

Image<uint8_t> bg = Image<uint8_t>(640,480,std::vector<uint8_t>(640*480,0));

WhiskerTracker::WhiskerTracker()
{
    janelia = JaneliaTracker();
    janelia_init = false;
    whiskers = std::vector<Whisker>{};
}

void WhiskerTracker::trace(const std::vector<uint8_t>& input, const std::pair<int,int> height_width) {

    if (this->janelia_init == false) {
        this->janelia.bank = LineDetector(this->janelia.config);
        this->janelia.half_space_bank = HalfSpaceDetector(this->janelia.config);
        this->janelia_init = true;
    }

    whiskers.clear();

    Image<uint8_t>img = Image<uint8_t>(std::get<1>(height_width),std::get<0>(height_width),input);
    std::vector<Whisker_Seg> j_segs = janelia.find_segments(1,img,bg);

    int whisker_count = 1;
    for (auto& w_seg : j_segs) {
        whiskers.push_back(Whisker(whisker_count++,std::move(w_seg.x),std::move(w_seg.y)));
    }
}

std::tuple<float,int> WhiskerTracker::get_nearest_whisker(float x_p, float y_p) {

    float nearest_distance = 1000.0;
    int whisker_id = 1;

    float current_d = 0.0f;
    int current_whisker_id = 0;

    for (auto& w : this->whiskers) {
        for (int i = 0; i < w.x.size(); i++) {
            current_d = sqrt(pow(x_p - w.x[i],2) + pow(y_p - w.y[i],2));
            if (current_d < nearest_distance) {
                nearest_distance = current_d;
                whisker_id = w.id;
            }
        }
    }

    return std::make_tuple(nearest_distance,whisker_id);
}
