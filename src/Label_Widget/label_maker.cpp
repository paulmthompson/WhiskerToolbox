
#include "label_maker.h"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

LabelMaker::LabelMaker() {
    this->label_name = "Label1";
    this->saveFilePath = "./test.json";
}

void LabelMaker::addLabel(int frame, int x, int y) {
    this->point_labels[frame] = label_point{x,y};

    printLabels();
}

void LabelMaker::printLabels() {
    for (auto i : this->point_labels) {
        std::cout << "Label on frame " << i.first << " at location x: " << i.second.x << " y: " << i.second.y << std::endl;
    }
}

void LabelMaker::saveLabelsJSON() {

    json j = json::array();

    for (auto i : this->getLabels()) {
        json json_object = json::object();

        json_object["image"] = makeFrameName(i.first);
        json_object["labels"][this->label_name] = {i.second.x, i.second.y};
        j.push_back(json_object);
    }

    std::cout << j.dump(2) << std::endl;

}

std::string LabelMaker::makeFrameName(int frame) {

    std::stringstream a;
    a << std::setw(6) << std::setfill('0') << frame;
    std::string frame_id = "scene" + a.str() + ".png";

    return frame_id;
}
