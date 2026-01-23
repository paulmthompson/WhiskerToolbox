#include "MediaPropertiesWidget.hpp"
#include "ui_MediaPropertiesWidget.h"

#include "Media_Widget/MediaWidgetState.hpp"
#include "DataManager/DataManager.hpp"

MediaPropertiesWidget::MediaPropertiesWidget(std::shared_ptr<MediaWidgetState> state,
                                               std::shared_ptr<DataManager> data_manager,
                                               Media_Window * media_window,
                                               QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _media_window(media_window) {
    ui->setupUi(this);

    _connectStateSignals();
}

MediaPropertiesWidget::~MediaPropertiesWidget() {
    delete ui;
}

void MediaPropertiesWidget::setMediaWindow(Media_Window * media_window) {
    _media_window = media_window;
    // Future: reconnect any widgets that need Media_Window reference
}

void MediaPropertiesWidget::_connectStateSignals() {
    if (!_state) {
        return;
    }

    // Future: Connect state signals to update UI when state changes
    // For example:
    // connect(_state.get(), &MediaWidgetState::featureEnabledChanged,
    //         this, &MediaPropertiesWidget::_onFeatureEnabledChanged);
}
