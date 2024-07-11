#ifndef LABEL_MAKER_HPP
#define LABEL_MAKER_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <filesystem>
#include <utility>
#include <sstream>

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
    void removeLabel(std::string frame_id) {_point_labels.erase(frame_id);};

    std::map<std::string, std::pair<image,label_point>> getLabels() const {return _point_labels;};

    std::stringstream saveLabelsJSON();
    std::stringstream saveLabelsCSV();
    void changeLabelName(std::string label_name) {_label_name = label_name;};

    image createImage(int height, int width, int frame_number, std::string frame_id, std::vector<uint8_t> data);

private:
    std::map<std::string, std::pair<image,label_point>> _point_labels;
    std::string _label_name;
    std::filesystem::path _saveFilePath;

    void _printLabels();
    std::string _makeFrameName(std::string frame_id);
};

#endif // LABEL_MAKER_HPP
