#ifndef LABEL_MAKER_H
#define LABEL_MAKER_H

#include <string>
#include <map>
#include <vector>


struct label_point {
    int x;
    int y;
};

class LabelMaker{
public:

    LabelMaker();
    void addLabel(int frame, int x, int y);
    void removeLabel(int frame) {this->point_labels.erase(frame);};
    std::map<int, label_point> getLabels() const {return this->point_labels;};

private:
    std::map<int, label_point> point_labels;
    std::vector<std::string> label_names;

    void printLabels();
};

#endif // LABEL_MAKER_H
