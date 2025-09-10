#include "MediaWidgetManager.hpp"

#include "DataManager/DataManager.hpp"
#include "Media_Widget/Media_Widget.hpp"
#include "Media_Widget/Media_Window/Media_Window.hpp"

#include <algorithm>
#include <iostream>

MediaWidgetManager::MediaWidgetManager(std::shared_ptr<DataManager> data_manager, QObject* parent)
    : QObject(parent)
    , _data_manager(std::move(data_manager)) {
}

Media_Widget* MediaWidgetManager::createMediaWidget(const std::string& id, QWidget* parent) {
    // Check if id already exists
    if (_media_widgets.find(id) != _media_widgets.end()) {
        std::cerr << "MediaWidgetManager: Media widget with id '" << id << "' already exists" << std::endl;
        return nullptr;
    }

    // Create Media_Widget - it will create its own Media_Window when setDataManager is called
    auto media_widget = std::make_unique<Media_Widget>(parent);
    media_widget->setDataManager(_data_manager);
    media_widget->updateMedia();

    // Get raw pointer before moving
    Media_Widget* widget_ptr = media_widget.get();

    // Store in map
    _media_widgets[id] = std::move(media_widget);

    // Connect signals
    _connectWidgetSignals(id, widget_ptr);

    emit mediaWidgetCreated(id, widget_ptr);

    std::cout << "MediaWidgetManager: Created media widget '" << id << "'" << std::endl;
    return widget_ptr;
}

bool MediaWidgetManager::removeMediaWidget(const std::string& id) {
    auto it = _media_widgets.find(id);
    if (it == _media_widgets.end()) {
        return false;
    }

    _media_widgets.erase(it);
    emit mediaWidgetRemoved(id);

    std::cout << "MediaWidgetManager: Removed media widget '" << id << "'" << std::endl;
    return true;
}

Media_Widget* MediaWidgetManager::getMediaWidget(const std::string& id) const {
    auto it = _media_widgets.find(id);
    if (it != _media_widgets.end()) {
        return it->second.get();
    }
    return nullptr;
}

Media_Window* MediaWidgetManager::getMediaWindow(const std::string& id) const {
    auto it = _media_widgets.find(id);
    if (it != _media_widgets.end()) {
        return it->second->getMediaWindow();
    }
    return nullptr;
}

std::vector<std::string> MediaWidgetManager::getMediaWidgetIds() const {
    std::vector<std::string> ids;
    ids.reserve(_media_widgets.size());
    
    for (const auto& [id, _] : _media_widgets) {
        ids.push_back(id);
    }
    
    return ids;
}

void MediaWidgetManager::setFeatureColorForAll(const std::string& feature, const std::string& hex_color) {
    for (const auto& [id, widget] : _media_widgets) {
        widget->setFeatureColor(feature, hex_color);
    }
}

void MediaWidgetManager::setFeatureColor(const std::string& widget_id, const std::string& feature, const std::string& hex_color) {
    auto widget = getMediaWidget(widget_id);
    if (widget) {
        widget->setFeatureColor(feature, hex_color);
    }
}

void MediaWidgetManager::loadFrameForAll(int frame_id) {
    for (const auto& [id, widget] : _media_widgets) {
        widget->LoadFrame(frame_id);
        emit frameLoaded(id, frame_id);
    }
}

void MediaWidgetManager::loadFrame(const std::string& widget_id, int frame_id) {
    auto widget = getMediaWidget(widget_id);
    if (widget) {
        widget->LoadFrame(frame_id);
        emit frameLoaded(widget_id, frame_id);
    }
}

void MediaWidgetManager::updateMediaForAll() {
    for (const auto& [id, widget] : _media_widgets) {
        widget->updateMedia();
    }
}

void MediaWidgetManager::_connectWidgetSignals(const std::string& id, Media_Widget* widget) {
    // Connect any relevant signals from the widget/window
    // For example, if Media_Window has signals that need to be forwarded
    // This can be extended based on specific requirements
    
    // Example of connecting a signal and re-emitting with widget ID
    // connect(widget->getMediaWindow(), &Media_Window::someSignal, this, [this, id](auto... args) {
    //     emit someSignalFromWidget(id, args...);
    // });
}
