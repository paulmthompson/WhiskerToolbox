#include "SpectrogramWidget.hpp"

#include "Core/SpectrogramState.hpp"
#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "ui_SpectrogramWidget.h"

SpectrogramWidget::SpectrogramWidget(std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::SpectrogramWidget),
      _state(nullptr) {
    ui->setupUi(this);
}

SpectrogramWidget::~SpectrogramWidget() {
    delete ui;
}

void SpectrogramWidget::setState(std::shared_ptr<SpectrogramState> state) {
    _state = state;
}

void SpectrogramWidget::_onTimeChanged(TimePosition /*position*/) {
    // Empty for now; can update spectrogram view when time changes from EditorRegistry.
}

SpectrogramState * SpectrogramWidget::state() {
    return _state.get();
}
