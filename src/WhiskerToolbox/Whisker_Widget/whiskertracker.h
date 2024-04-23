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

   std::tuple<float,float> getWhiskerPad() const {return _whisker_pad;};
   void setWhiskerPad(float w_x, float w_y) {_whisker_pad = std::make_tuple(w_x,w_y);};

   std::vector<Whisker> whiskers;

private:
   JaneliaTracker _janelia;
    bool _janelia_init;
   float _whisker_length_threshold;
    std::tuple<float,float> _whisker_pad;

   void _removeDuplicates(std::vector<float>& scores);
    void _alignWhiskerToFollicle(Whisker& whisker);
   float _calculateWhiskerLength(const Whisker& whisker);

};




#endif // WHISKERTRACKER_H
