#include "MediaDisplayManager.hpp"

#include "../Media_Widget/Media_Widget.hpp"
#include "../Media_Window/Media_Window.hpp"
#include "../DataManager/DataManager.hpp"

#include <QVBoxLayout>
#include <QUuid>

MediaDisplayManager::MediaDisplayManager(std::shared_ptr<DataManager> data_manager, 
                                        QWidget* parent)
    : QWidget(parent), _data_manager(std::move(data_manager)) {
    
    _generateDisplayId();
    
    // Create the media scene first
    _media_scene = std::make_unique<Media_Window>(_data_manager, this);
    
    // Create the media widget and connect it to the scene
    _media_widget = std::make_unique<Media_Widget>(this);
    _media_widget->setDataManager(_data_manager);
    _media_widget->setScene(_media_scene.get());
    
    // Set up the layout to contain the media widget
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_media_widget.get());
    
    _setupConnections();
    
    // Initialize the display
    updateDisplay();
}

MediaDisplayManager::~MediaDisplayManager() = default;

void MediaDisplayManager::updateDisplay() {
    _media_widget->updateMedia();
    emit displayContentChanged();
}

void MediaDisplayManager::loadFrame(int frame_id) {
    _media_widget->LoadFrame(frame_id);
    emit displayContentChanged();
}

void MediaDisplayManager::setFeatureColor(const std::string& feature, const std::string& hex_color) {
    _media_widget->setFeatureColor(feature, hex_color);
    emit displayContentChanged();
}

void MediaDisplayManager::_setupConnections() {
    // For now, we'll use a simpler approach for selection detection
    // This could be enhanced with custom focus tracking later
    
    // You can add more connections here as needed for coordination
}

void MediaDisplayManager::_generateDisplayId() {
    // Generate a unique ID for this display
    _display_id = "media_display_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}
