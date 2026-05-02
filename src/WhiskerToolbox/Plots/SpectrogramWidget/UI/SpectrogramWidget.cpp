#include "SpectrogramWidget.hpp"

#include "Core/SpectrogramState.hpp"
#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "ui_SpectrogramWidget.h"

#include <set>
#include <utility>

SpectrogramWidget::SpectrogramWidget(std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager(std::move(std::move(data_manager))),
      ui(new Ui::SpectrogramWidget),
      _state(nullptr) {
    ui->setupUi(this);
}

SpectrogramWidget::~SpectrogramWidget() {
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void SpectrogramWidget::setState(std::shared_ptr<SpectrogramState> state) {
    _state = std::move(state);

    // Register DataManager-level observer to detect key removal
    if (_data_manager && _dm_observer_id == -1) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _pruneRemovedKeys();
        }, "SpectrogramWidget");
    }
}

void SpectrogramWidget::_onTimeChanged(const TimePosition& /*position*/) {
    // Empty for now; can update spectrogram view when time changes from EditorRegistry.
}

void SpectrogramWidget::_pruneRemovedKeys() {
    if (!_state || !_data_manager) {
        return;
    }
    auto const all_keys = _data_manager->getAllKeys();
    std::set<std::string> const key_set(all_keys.begin(), all_keys.end());

    // Clear analog signal key if removed
    auto const signal_key = _state->getAnalogSignalKey().toStdString();
    if (!signal_key.empty() && key_set.find(signal_key) == key_set.end()) {
        _state->setAnalogSignalKey(QString());
    }
}

SpectrogramState * SpectrogramWidget::state() {
    return _state.get();
}
