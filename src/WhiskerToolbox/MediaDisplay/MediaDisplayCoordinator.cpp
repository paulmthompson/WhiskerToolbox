#include "MediaDisplayCoordinator.hpp"
#include "MediaDisplayManager.hpp"
#include "../Media_Window/Media_Window.hpp"
#include "../DataManager/DataManager.hpp"

#include <algorithm>

MediaDisplayCoordinator::MediaDisplayCoordinator(std::shared_ptr<DataManager> data_manager, 
                                               QObject* parent)
    : QObject(parent), _data_manager(std::move(data_manager)) {
}

MediaDisplayCoordinator::~MediaDisplayCoordinator() = default;

MediaDisplayManager* MediaDisplayCoordinator::createMediaDisplay() {
    auto display = std::make_unique<MediaDisplayManager>(_data_manager);
    auto* display_ptr = display.get();
    
    std::string display_id = display->getId();
    
    _setupDisplayConnections(display_ptr);
    
    _displays[display_id] = std::move(display);
    
    // Set as active if it's the first display
    if (_active_display_id.empty()) {
        _active_display_id = display_id;
    }
    
    emit displayCreated(display_id);
    
    return display_ptr;
}

void MediaDisplayCoordinator::removeMediaDisplay(const std::string& display_id) {
    auto it = _displays.find(display_id);
    if (it != _displays.end()) {
        // If removing the active display, select another one
        if (_active_display_id == display_id) {
            _active_display_id.clear();
            for (const auto& [id, display] : _displays) {
                if (id != display_id) {
                    _active_display_id = id;
                    break;
                }
            }
        }
        
        _displays.erase(it);
        emit displayRemoved(display_id);
        
        if (!_active_display_id.empty()) {
            emit activeDisplayChanged(_active_display_id);
        }
    }
}

std::vector<MediaDisplayManager*> MediaDisplayCoordinator::getActiveDisplays() const {
    std::vector<MediaDisplayManager*> displays;
    displays.reserve(_displays.size());
    
    for (const auto& [id, display] : _displays) {
        displays.push_back(display.get());
    }
    
    return displays;
}

MediaDisplayManager* MediaDisplayCoordinator::getDisplay(const std::string& display_id) const {
    auto it = _displays.find(display_id);
    return (it != _displays.end()) ? it->second.get() : nullptr;
}

MediaDisplayManager* MediaDisplayCoordinator::getActiveDisplay() const {
    return getDisplay(_active_display_id);
}

std::vector<Media_Window*> MediaDisplayCoordinator::getAllScenesForExport() const {
    std::vector<Media_Window*> scenes;
    scenes.reserve(_displays.size());
    
    for (const auto& [id, display] : _displays) {
        scenes.push_back(display->getScene());
    }
    
    return scenes;
}

std::vector<Media_Window*> MediaDisplayCoordinator::getSelectedScenesForExport(
    const std::vector<std::string>& display_ids) const {
    
    std::vector<Media_Window*> scenes;
    scenes.reserve(display_ids.size());
    
    for (const auto& display_id : display_ids) {
        auto* display = getDisplay(display_id);
        if (display) {
            scenes.push_back(display->getScene());
        }
    }
    
    return scenes;
}

void MediaDisplayCoordinator::synchronizeFrame(int frame_id) {
    for (const auto& [id, display] : _displays) {
        display->loadFrame(frame_id);
    }
}

void MediaDisplayCoordinator::synchronizeFeatureColor(const std::string& feature, 
                                                    const std::string& hex_color) {
    for (const auto& [id, display] : _displays) {
        display->setFeatureColor(feature, hex_color);
    }
}

void MediaDisplayCoordinator::_onDisplaySelected(const std::string& display_id) {
    if (_active_display_id != display_id) {
        _active_display_id = display_id;
        emit activeDisplayChanged(display_id);
    }
}

void MediaDisplayCoordinator::_onDisplayContentChanged() {
    // This could be used for coordinating updates across displays if needed
    // For now, we'll leave it for future extension
}

void MediaDisplayCoordinator::_setupDisplayConnections(MediaDisplayManager* display) {
    connect(display, &MediaDisplayManager::displaySelected,
            this, &MediaDisplayCoordinator::_onDisplaySelected);
    
    connect(display, &MediaDisplayManager::displayContentChanged,
            this, &MediaDisplayCoordinator::_onDisplayContentChanged);
}
