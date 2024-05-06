#ifndef WHISKERTRACKER_H
#define WHISKERTRACKER_H

#include <unordered_map>
#include <vector>
#include <string>
#include <map>

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

   void trace(const std::vector<uint8_t>& image,const int image_height, const int image_width);
   std::tuple<float,int> get_nearest_whisker(float x_p, float y_p);
   std::map<int,std::vector<Whisker>> load_janelia_whiskers(const std::string filename);


   float getWhiskerLengthThreshold() const {return _whisker_length_threshold;};
   void setWhiskerLengthThreshold(float length_threshold) {_whisker_length_threshold = length_threshold;};

   float getWhiskerPadRadius() const {return _whisker_pad_radius;};
   void setWhiskerPadRadius(float whisker_pad_radius) {_whisker_pad_radius = whisker_pad_radius;};

   std::tuple<float,float> getWhiskerPad() const {return _whisker_pad;};
   void setWhiskerPad(float w_x, float w_y) {_whisker_pad = std::make_tuple(w_x,w_y);};

   enum JaneliaParameter {
       SEED_ON_GRID_LATTICE_SPACING,
       SEED_SIZE_PX,
       SEED_ITERATIONS,
       SEED_ITERATION_THRESH,
       SEED_ACCUM_THRESH,
       SEED_THRESH,
       HAT_RADIUS,
       MIN_LEVEL,
       MIN_SIZE,
       TLEN,
       OFFSET_STEP,
       ANGLE_STEP,
       WIDTH_STEP,
       WIDTH_MIN,
       WIDTH_MAX,
       MIN_SIGNAL,
       MAX_DELTA_ANGLE,
       MAX_DELTA_WIDTH,
       MAX_DELTA_OFFSET,
       HALF_SPACE_ASSYMETRY_THRESH,
       HALF_SPACE_TUNNELING_MAX_MOVES
   };

   void changeJaneliaParameter(JaneliaParameter parameter, float value);

   std::vector<Whisker> whiskers;

private:
   JaneliaTracker _janelia;
    bool _janelia_init;
   float _whisker_length_threshold;
    float _whisker_pad_radius;
    std::tuple<float,float> _whisker_pad;

   void _removeDuplicates(std::vector<float>& scores);
    void _alignWhiskerToFollicle(Whisker& whisker);
   float _calculateWhiskerLength(const Whisker& whisker);
    void _removeWhiskersByWhiskerPadRadius();
   void _eraseWhiskers(std::vector<int>& erase_inds);
    void _reinitializeJanelia();

};




#endif // WHISKERTRACKER_H
