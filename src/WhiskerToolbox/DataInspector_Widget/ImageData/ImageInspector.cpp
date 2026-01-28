#include "ImageInspector.hpp"

#include "Image_Widget.hpp"

#include <QVBoxLayout>

ImageInspector::ImageInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

ImageInspector::~ImageInspector() {
    removeCallbacks();
}

void ImageInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_image_widget) {
        _image_widget->setActiveKey(key);
    }
}

void ImageInspector::removeCallbacks() {
    if (_image_widget) {
        _image_widget->removeCallbacks();
    }
}

void ImageInspector::updateView() {
    if (_image_widget) {
        _image_widget->updateTable();
    }
}

void ImageInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped Image_Widget
    _image_widget = std::make_unique<Image_Widget>(dataManager(), this);

    layout->addWidget(_image_widget.get());
}

void ImageInspector::_connectSignals() {
    if (_image_widget) {
        // Forward frame selection signal
        connect(_image_widget.get(), &Image_Widget::frameSelected,
                this, &ImageInspector::frameSelected);
    }
}
