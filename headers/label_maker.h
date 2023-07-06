#ifndef LABEL_MAKER_H
#define LABEL_MAKER_H

#include <string>
#include <map>
#include <vector>
#include <filesystem>


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
    void saveLabelsJSON();

private:
    std::map<int, label_point> point_labels;
    std::string label_name;
    std::filesystem::path saveFilePath;

    void printLabels();
    std::string makeFrameName(int frame);
};

#endif // LABEL_MAKER_H
