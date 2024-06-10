#include "whiskertracker.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include "io.hpp"

Image<uint8_t> bg = Image<uint8_t>(640,480,std::vector<uint8_t>(640*480,0));

WhiskerTracker::WhiskerTracker() :
    _whisker_length_threshold{75.0},
    _whisker_pad_radius{150.0},
    _janelia_init{false},
    _whisker_pad{0.0f, 0.0f}
{
    _janelia = JaneliaTracker();
    whiskers = std::vector<Whisker>{};
}

void WhiskerTracker::trace(const std::vector<uint8_t>& image, const int image_height, const int image_width) {

    if (this->_janelia_init == false) {
        this->_janelia.bank = LineDetector(this->_janelia.config);
        this->_janelia.half_space_bank = HalfSpaceDetector(this->_janelia.config);
        this->_janelia_init = true;
    }

    whiskers.clear();

    Image<uint8_t>img = Image<uint8_t>(image_width,image_height,image);
    std::vector<Whisker_Seg> j_segs = _janelia.find_segments(1,img,bg);

    std::vector<float> scores = std::vector<float>();
    int whisker_count = 1;
    for (auto& w_seg : j_segs) {
        auto whisker = Whisker(whisker_count++,std::move(w_seg.x),std::move(w_seg.y));
        if (_calculateWhiskerLength(whisker) > _whisker_length_threshold) {
            _alignWhiskerToFollicle(whisker);
            whiskers.push_back(whisker);
            scores.push_back(std::accumulate(w_seg.scores.begin(),w_seg.scores.end(),0.0) / static_cast<float>(w_seg.scores.size()));
        }
    }

    _removeDuplicates(scores);
    _removeWhiskersByWhiskerPadRadius();
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

std::map<int,std::vector<Whisker>> WhiskerTracker::load_janelia_whiskers(const std::string filename)
{
    auto j_segs = load_binary_data(filename);

    auto output_whiskers = std::map<int,std::vector<Whisker>>();

    for (auto& w_seg : j_segs) {

        if (output_whiskers.find(w_seg.time) == output_whiskers.end()) { // Key doesn't exist
            output_whiskers[w_seg.time] = std::vector<Whisker>();
        }

         output_whiskers[w_seg.time].push_back(Whisker(w_seg.id,std::move(w_seg.x),std::move(w_seg.y)));

    }

    return output_whiskers;
}

float WhiskerTracker::_calculateWhiskerLength(const Whisker& whisker)
{
    float length = 0.0;

    for (int i = 1; i < whisker.x.size(); i++)
    {
        length += sqrt(pow((whisker.x[i] - whisker.x[i-1]),2) + pow((whisker.y[i] - whisker.y[i-1]),2));
    }

    return length;
}

/**
 * @brief WhiskerTracker::_alignWhiskerToFollicle
 *
 * Measures the distance between the Point at one end of a whisker and Point
 * at the other end. The whisker is then flipped so that the first index is closest
 * to the follicle
 *
 *
 * @param whisker whisker to be checked
 */
void WhiskerTracker::_alignWhiskerToFollicle(Whisker& whisker)
{
    auto follicle_x = std::get<0>(_whisker_pad);
    auto follicle_y = std::get<1>(_whisker_pad);

    auto start_distance = sqrt(pow((whisker.x[0] - follicle_x),2) + pow((whisker.y[0] - follicle_y),2));

    auto end_distance = sqrt(pow((whisker.x.back() - follicle_x),2) + pow((whisker.y.back() - follicle_y),2));

    if (start_distance > end_distance) {
        std::reverse(whisker.x.begin(),whisker.x.end());
        std::reverse(whisker.y.begin(),whisker.y.end());
    }
}

void WhiskerTracker::changeJaneliaParameter(JaneliaParameter parameter, float value)
{
    switch (parameter) {
        case SEED_ON_GRID_LATTICE_SPACING: {
            _janelia.config._lattice_spacing = static_cast<int>(value);
            break;
        }
        case SEED_SIZE_PX: {
            _janelia.config._maxr = static_cast<int>(value);
            break;
        }
        case SEED_ITERATIONS: {
            _janelia.config._maxiter = static_cast<int>(value);
            break;
        }
        case SEED_ITERATION_THRESH: {
            _janelia.config._iteration_thres = value;
            break;
        }
        case SEED_ACCUM_THRESH: {
            _janelia.config._accum_thres = value;
            break;
        }
        case SEED_THRESH: {
            _janelia.config._accum_thres = value;
            break;
        }
        case HAT_RADIUS: {

        }
        case MIN_LEVEL: {

        }
        case MIN_SIZE: {

        }
        case TLEN: {
            _janelia.config._tlen = value;
            _reinitializeJanelia();
            break;
        }
        case OFFSET_STEP: {
            _janelia.config._offset_step = value;
            _reinitializeJanelia();
            break;
        }
        case ANGLE_STEP: {
            _janelia.config._angle_step = value;
            _reinitializeJanelia();
            break;
        }
        case WIDTH_STEP: {
            _janelia.config._width_step = value;
            _reinitializeJanelia();
        }
        case WIDTH_MIN: {
            // Must be multiple of width step
            _janelia.config._width_min = value;
            _reinitializeJanelia();
            break;
        }
        case WIDTH_MAX: {
            _janelia.config._width_max = value;
            _reinitializeJanelia();
            break;
        }
        case MIN_SIGNAL: {
            _janelia.config._min_signal = value;
            _reinitializeJanelia();
            break;
        }
        case MAX_DELTA_ANGLE: {
            _janelia.config._max_delta_angle = value;
            _reinitializeJanelia();
            break;
        }
        case MAX_DELTA_WIDTH: {
            _janelia.config._max_delta_width = value;
            _reinitializeJanelia();
            break;
        }
        case MAX_DELTA_OFFSET: {
            _janelia.config._max_delta_offset = value;
            _reinitializeJanelia();
            break;
        }
        case HALF_SPACE_ASSYMETRY_THRESH: {
            _janelia.config._half_space_assymetry = value;
            _reinitializeJanelia();
            break;
        }
        case HALF_SPACE_TUNNELING_MAX_MOVES: {
            _janelia.config._half_space_tunneling_max_moves = value;
            _reinitializeJanelia();
            break;
        }
    }
}

void WhiskerTracker::_removeDuplicates(std::vector<float>& scores)
{

    struct correlation_matrix {
        int i;
        int j;
        double corr;
    };

    auto minimum_size = 20;
    auto correlation_threshold = 20.0;

    auto cor_mat = std::vector<correlation_matrix>();

    for (int i = 0; i<whiskers.size(); i++)
    {
        if (whiskers[i].x.size() < minimum_size)
        {
            continue;
        }
        for (int j=i+1; j<whiskers.size(); j++)
        {
            if  (whiskers[j].x.size() < minimum_size)
            {
                continue;
            }

            auto this_cor = 0.0;
            for (int k=0; k < minimum_size; k++) {
                this_cor += sqrt(pow(whiskers[i].x.end()[-k - 1] - whiskers[j].x.end()[-k - 1],2) +
                                pow(whiskers[i].y.end()[-k - 1] - whiskers[j].y.end()[-k - 1],2));
            }

            cor_mat.push_back(correlation_matrix{i,j,this_cor});
        }
    }

    std::vector<int> erase_inds = std::vector<int>();
    for (int i = 0; i< cor_mat.size(); i++)
    {
        if (cor_mat[i].corr < correlation_threshold)
        {
            std::cout << "Whiskers " << cor_mat[i].i << " and " << cor_mat[i].j << " are the same" << std::endl;

            if (scores[cor_mat[i].i] > scores[cor_mat[i].j])
            {
                erase_inds.push_back(cor_mat[i].j);
            } else {
                erase_inds.push_back(cor_mat[i].i);
            }
        }
    }

    _eraseWhiskers(erase_inds);
}

void WhiskerTracker::_removeWhiskersByWhiskerPadRadius()
{

    std::vector<int> erase_inds = std::vector<int>();
    auto follicle_x = std::get<0>(_whisker_pad);
    auto follicle_y = std::get<1>(_whisker_pad);

    for (int i = 0; i < whiskers.size(); i++ ) {
        auto distance_to_follicle = sqrt(pow(whiskers[i].x[0] - follicle_x,2) +
                                         pow(whiskers[i].y[0] - follicle_y,2));

        if (distance_to_follicle > _whisker_pad_radius) {
            erase_inds.push_back(i);
        }
    }

    _eraseWhiskers(erase_inds);
}

void WhiskerTracker::_eraseWhiskers(std::vector<int>& erase_inds)
{
    std::sort(erase_inds.begin(), erase_inds.end(),std::greater<int>());
    auto last = std::unique(erase_inds.begin(), erase_inds.end());
    erase_inds.erase(last, erase_inds.end());

    for (auto& erase_ind : erase_inds) {
        whiskers.erase(whiskers.begin() + erase_ind);
    }
}

void WhiskerTracker::_reinitializeJanelia()
{
    _janelia.bank = LineDetector(_janelia.config);
    _janelia.half_space_bank = HalfSpaceDetector(_janelia.config);
    _janelia_init = true;
}
