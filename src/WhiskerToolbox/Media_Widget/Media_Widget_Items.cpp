

#include "Media_Widget_Items.hpp"

#include "ui_Media_Widget_Items.h"

#include "DataManager.hpp"

#include "Media_Window/Media_Window.hpp"

class PointData;

enum class Media_Item_Type {
    POINT,
    LINE,
    MASK
};

struct Media_Item {
    std::string name;
    Media_Item_Type type;
    bool visible;
    std::string hex_color;
    float alpha;
};

Media_Widget_Items::Media_Widget_Items(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget *parent) :
    QWidget(parent),
    _data_manager{data_manager},
    _scene{scene},
    ui(new Ui::Media_Widget_Items)
{
    ui->setupUi(this);
}

Media_Widget_Items::~Media_Widget_Items() {
    delete ui;
}

void Media_Widget_Items::_getPointItems()
{
    auto keys = _data_manager->getKeys<PointData>();

}

void Media_Widget_Items::_getLineItems()
{
    auto keys = _data_manager->getKeys<LineData>();
}


void Media_Widget_Items::_getMaskItems()
{
    auto keys = _data_manager->getKeys<MaskData>();
}
