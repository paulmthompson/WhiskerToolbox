
#include "label_maker.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

LabelMaker::LabelMaker() {
    this->label_name = "Label1";
    this->saveFilePath = "./test.json";
}

void LabelMaker::addLabel(image img, int x, int y) {

    std::pair<image,label_point> pair = std::make_pair(img,label_point{x,y});
    this->point_labels[img.frame_id] = pair;

    printLabels();
}

void LabelMaker::printLabels() {
    for (auto& [frame_name,label ] : this->point_labels) {
        auto& [img, point] = label;
        std::cout << "Label on frame " << frame_name << " at location x: " << point.x << " y: " << point.y << std::endl;
    }
}

std::stringstream LabelMaker::saveLabelsJSON() {

    json j = json::array();

    for (auto& [frame_name,label ]: this->getLabels()) {

        auto& [img, point] = label;

        json json_object = json::object();

        json_object["image"] = makeFrameName(frame_name);
        json_object["labels"][this->label_name] = {point.x, point.y};
        j.push_back(json_object);
    }

    std::stringstream out_stream;
    out_stream << j.dump(2);
    return out_stream;

}

std::string LabelMaker::makeFrameName(std::string frame_id) {

    //We create a 7 digit number, padding with leading zeros
    std::stringstream a;
    a << std::setw(7) << std::setfill('0') << frame_id;

    std::string frame_name = a.str();

    if (frame_name.substr(0,5) != "scene") {
        frame_name = "scene" + frame_name;
    }

    if (frame_name.substr(frame_name.length() - 4) != ".png") {
        frame_name = frame_name + ".png";
    }

    return frame_name;
}

image LabelMaker::createImage(int height, int width, int frame_number, std::string frame_id, std::vector<uint8_t> data) {
    return image(data, height, width, frame_number, frame_id);
}
