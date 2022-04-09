#ifndef WHISKERTRACKER_H
#define WHISKERTRACKER_H

#include <unordered_map>
#include <vector>

struct Whisker {
    int id;
    std::vector<float> x;
    std::vector<float> y;
};

class WhiskerTracker {

public:
    WhiskerTracker();

private:
    std::unordered_map<int, Whisker> woi;

};




#endif // WHISKERTRACKER_H
