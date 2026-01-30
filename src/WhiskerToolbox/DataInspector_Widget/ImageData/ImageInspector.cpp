#include "ImageInspector.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include <iostream>

ImageInspector::ImageInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
}

ImageInspector::~ImageInspector() {
    removeCallbacks();
}

void ImageInspector::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        return;
    }
    
    removeCallbacks();
    _active_key = key;
    _assignCallbacks();
}

void ImageInspector::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void ImageInspector::updateView() {
    // ImageInspector doesn't maintain its own UI - ImageDataView handles the table
    // This method is called when data changes, but the actual view update
    // is handled by ImageDataView through its own callbacks
}

void ImageInspector::_assignCallbacks() {
    if (!_active_key.empty()) {
        auto media_data = dataManager()->getData<MediaData>(_active_key);
        if (media_data) {
            _callback_id = dataManager()->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
        } else {
            std::cerr << "ImageInspector: No MediaData found for key '" << _active_key 
                      << "' to attach callback." << std::endl;
        }
    }
}

void ImageInspector::_onDataChanged() {
    // Notify that data has changed - ImageDataView will handle the actual view update
    // through its own callback mechanism
    updateView();
}
