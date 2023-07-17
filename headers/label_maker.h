#ifndef LABEL_MAKER_H
#define LABEL_MAKER_H

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <filesystem>
#include <utility>

/*
Label maker is used for associating labels with specific images it gathers from media player


*/

struct image {
    std::vector<uint8_t> data;
    int height;
    int width;
    int media_window_frame_number;
    std::string frame_id;
    image() {}
    image(std::vector<uint8_t> _data, int _height, int _width, int _media_window_frame_number, std::string _frame_id)
    {
        data = _data;
        height = _height;
        width = _width;
        media_window_frame_number = _media_window_frame_number;
        frame_id = _frame_id;
    }
};

struct label_point {
    int x;
    int y;
};

class LabelMaker{
public:

    LabelMaker();

    void addLabel(image img, int x, int y); // We should probably send an image here that can be labeled
    void removeLabel(std::string frame_id) {this->point_labels.erase(frame_id);};

    std::map<std::string, std::pair<image,label_point>> getLabels() const {return this->point_labels;};

    std::stringstream saveLabelsJSON();
    void changeLabelName(std::string label_name) {this->label_name = label_name;};

    image createImage(int height, int width, int frame_number, std::string frame_id, std::vector<uint8_t> data);

private:
    std::map<std::string, std::pair<image,label_point>> point_labels;
    std::string label_name;
    std::filesystem::path saveFilePath;

    void printLabels();
    std::string makeFrameName(std::string frame_id);
};

#endif // LABEL_MAKER_H
