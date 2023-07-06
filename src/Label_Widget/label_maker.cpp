
#include "label_maker.h"

#include <iostream>

LabelMaker::LabelMaker() {

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
