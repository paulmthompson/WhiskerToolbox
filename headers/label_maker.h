#ifndef LABEL_MAKER_H
#define LABEL_MAKER_H

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <filesystem>


struct label_point {
    int x;
    int y;
};

class LabelMaker{
public:

    LabelMaker();
    void addLabel(std::string frame_id, int x, int y);
    void removeLabel(std::string frame_id) {this->point_labels.erase(frame_id);};
    std::map<std::string, label_point> getLabels() const {return this->point_labels;};
    std::stringstream saveLabelsJSON();
    void changeLabelName(std::string label_name) {this->label_name = label_name;};

private:
    std::map<std::string, label_point> point_labels;
    std::string label_name;
    std::filesystem::path saveFilePath;

    void printLabels();
    std::string makeFrameName(std::string frame_id);
};

#endif // LABEL_MAKER_H
