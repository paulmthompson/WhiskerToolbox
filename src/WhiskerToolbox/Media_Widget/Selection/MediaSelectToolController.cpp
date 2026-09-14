/**
 * @file MediaSelectToolController.cpp
 * @brief Orchestrates unified Select-tool canvas selection and properties sync
 */

#include "MediaSelectToolController.hpp"

#include "Core/MediaWidgetState.hpp"
#include "Rendering/Media_Window/Media_Window.hpp"

MediaSelectToolController::MediaSelectToolController(QObject * parent)
    : QObject(parent) {}

void MediaSelectToolController::setMediaWindow(Media_Window * window) {
    if (_window) {
        disconnect(_window, &Media_Window::groupSelectionInteracted, this, nullptr);
    }

    _window = window;

    if (_window) {
        connect(_window, &Media_Window::groupSelectionInteracted,
                this, &MediaSelectToolController::_onCanvasSelectionChanged);
        _window->setUnifiedSelectionEnabled(_active);
    }
}

void MediaSelectToolController::setState(MediaWidgetState * state) {
    _state = state;
}

void MediaSelectToolController::setActive(bool active) {
    if (_active == active) {
        return;
    }

    _active = active;

    if (_window) {
        _window->setUnifiedSelectionEnabled(_active);
    }
}

void MediaSelectToolController::_onCanvasSelectionChanged() {
    if (!_active || !_window || !_state || !_window->hasSelections()) {
        return;
    }

    std::string const & key = _window->selectedDataKey();
    if (key.empty()) {
        return;
    }

    _state->setDisplayedDataKey(QString::fromStdString(key));
}
