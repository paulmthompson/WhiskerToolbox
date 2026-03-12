#include "RegionSelectionPanel.hpp"

#include "MLCoreWidgetState.hpp"
#include "ui_RegionSelectionPanel.h"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <QTimer>

#include <algorithm>
#include <numeric>

RegionSelectionPanel::RegionSelectionPanel(
    std::shared_ptr<MLCoreWidgetState> state,
    std::shared_ptr<DataManager> data_manager,
    RegionMode mode,
    QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::RegionSelectionPanel)
    , _state(std::move(state))
    , _data_manager(std::move(data_manager))
    , _mode(mode) {
    ui->setupUi(this);

    // Set title based on mode
    if (_mode == RegionMode::Training) {
        ui->regionGroupBox->setTitle(QStringLiteral("Training Region"));
    } else {
        ui->regionGroupBox->setTitle(QStringLiteral("Prediction Region"));
    }

    _setupConnections();
    _registerDataManagerObserver();

    // Initial population
    refreshRegionList();

    // Restore selection from state
    std::string saved_key = _readKeyFromState();
    if (saved_key.empty()) {
        // Empty key means "all frames"
        ui->allFramesCheckBox->setChecked(true);
        ui->regionComboBox->setEnabled(false);
    } else {
        ui->allFramesCheckBox->setChecked(false);
        ui->regionComboBox->setEnabled(true);
        for (int i = 0; i < ui->regionComboBox->count(); ++i) {
            QVariant data = ui->regionComboBox->itemData(i);
            if (data.isValid() && data.toString() == QString::fromStdString(saved_key)) {
                ui->regionComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
}

RegionSelectionPanel::~RegionSelectionPanel() {
    if (_dm_observer_id >= 0 && _data_manager) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

bool RegionSelectionPanel::hasValidSelection() const {
    return _valid;
}

std::string RegionSelectionPanel::selectedRegionKey() const {
    if (ui->allFramesCheckBox->isChecked()) {
        return {};
    }
    QString text = ui->regionComboBox->currentText();
    if (text.isEmpty()) {
        return {};
    }
    // Get the actual key from item data (display text may include count info)
    int idx = ui->regionComboBox->currentIndex();
    if (idx >= 0) {
        QVariant data = ui->regionComboBox->itemData(idx);
        if (data.isValid()) {
            return data.toString().toStdString();
        }
    }
    return text.toStdString();
}

bool RegionSelectionPanel::isAllFramesChecked() const {
    return ui->allFramesCheckBox->isChecked();
}

void RegionSelectionPanel::refreshRegionList() {
    if (!_data_manager) {
        return;
    }

    QString current_selection;
    int current_idx = ui->regionComboBox->currentIndex();
    if (current_idx >= 0) {
        QVariant data = ui->regionComboBox->itemData(current_idx);
        if (data.isValid()) {
            current_selection = data.toString();
        } else {
            current_selection = ui->regionComboBox->currentText();
        }
    }

    ui->regionComboBox->blockSignals(true);
    ui->regionComboBox->clear();

    // Add empty sentinel item
    ui->regionComboBox->addItem(QString{});

    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    std::sort(interval_keys.begin(), interval_keys.end());

    for (auto const & key : interval_keys) {
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (series) {
            QString display = QStringLiteral("%1 (%2 intervals)")
                                  .arg(QString::fromStdString(key))
                                  .arg(series->size());
            ui->regionComboBox->addItem(display, QString::fromStdString(key));
        } else {
            ui->regionComboBox->addItem(QString::fromStdString(key),
                                        QString::fromStdString(key));
        }
    }

    // Restore previous selection if still available
    int idx = -1;
    for (int i = 0; i < ui->regionComboBox->count(); ++i) {
        QVariant data = ui->regionComboBox->itemData(i);
        if (data.isValid() && data.toString() == current_selection) {
            idx = i;
            break;
        }
        if (ui->regionComboBox->itemText(i) == current_selection) {
            idx = i;
            break;
        }
    }

    if (idx != -1) {
        ui->regionComboBox->setCurrentIndex(idx);
    } else {
        ui->regionComboBox->setCurrentIndex(0);
    }

    ui->regionComboBox->blockSignals(false);

    _updateRegionInfo();
}

void RegionSelectionPanel::_onRegionComboChanged(int index) {
    QString key_str;
    if (index >= 0) {
        QVariant data = ui->regionComboBox->itemData(index);
        if (data.isValid()) {
            key_str = data.toString();
        } else {
            key_str = ui->regionComboBox->itemText(index);
        }
    }

    std::string key = key_str.toStdString();
    _syncToState(key);
    _updateRegionInfo();
    emit regionSelectionChanged(key_str);
}

void RegionSelectionPanel::_onAllFramesToggled(bool checked) {
    ui->regionComboBox->setEnabled(!checked);

    if (checked) {
        // Clear the state key — empty means "all frames"
        _syncToState({});
        _updateRegionInfo();
        emit regionSelectionChanged(QString{});
    } else {
        // Re-enable and apply current combo selection
        _onRegionComboChanged(ui->regionComboBox->currentIndex());
    }

    emit allFramesToggled(checked);
}

void RegionSelectionPanel::_setupConnections() {
    connect(ui->regionComboBox, &QComboBox::currentIndexChanged,
            this, &RegionSelectionPanel::_onRegionComboChanged);

    connect(ui->refreshButton, &QPushButton::clicked,
            this, &RegionSelectionPanel::refreshRegionList);

    connect(ui->allFramesCheckBox, &QCheckBox::toggled,
            this, &RegionSelectionPanel::_onAllFramesToggled);

    // React to external state changes (e.g., from JSON restore)
    if (_state) {
        auto state_changed_signal =
            (_mode == RegionMode::Training)
                ? &MLCoreWidgetState::trainingRegionKeyChanged
                : &MLCoreWidgetState::predictionRegionKeyChanged;

        connect(_state.get(), state_changed_signal,
                this, [this](QString const & key) {
                    if (key.isEmpty()) {
                        ui->allFramesCheckBox->blockSignals(true);
                        ui->allFramesCheckBox->setChecked(true);
                        ui->allFramesCheckBox->blockSignals(false);
                        ui->regionComboBox->setEnabled(false);
                        _updateRegionInfo();
                        return;
                    }

                    ui->allFramesCheckBox->blockSignals(true);
                    ui->allFramesCheckBox->setChecked(false);
                    ui->allFramesCheckBox->blockSignals(false);
                    ui->regionComboBox->setEnabled(true);

                    // Find and select the matching item
                    for (int i = 0; i < ui->regionComboBox->count(); ++i) {
                        QVariant data = ui->regionComboBox->itemData(i);
                        if (data.isValid() && data.toString() == key) {
                            ui->regionComboBox->blockSignals(true);
                            ui->regionComboBox->setCurrentIndex(i);
                            ui->regionComboBox->blockSignals(false);
                            _updateRegionInfo();
                            return;
                        }
                    }
                    // Key not found — may need refresh
                    refreshRegionList();
                });
    }
}

void RegionSelectionPanel::_updateRegionInfo() {
    // "All frames" mode
    if (ui->allFramesCheckBox->isChecked()) {
        ui->regionInfoLabel->setText(
            QStringLiteral("<b>All frames</b> will be used"));
        ui->validationLabel->setText(QString{});

        bool was_valid = _valid;
        _valid = true;
        if (!was_valid) {
            emit validityChanged(true);
        }
        return;
    }

    std::string key = selectedRegionKey();

    if (key.empty() || !_data_manager) {
        ui->regionInfoLabel->setText(QStringLiteral("No region selected"));
        ui->validationLabel->setText(QString{});

        bool was_valid = _valid;
        _valid = false;
        if (was_valid) {
            emit validityChanged(false);
        }
        return;
    }

    auto series = _data_manager->getData<DigitalIntervalSeries>(key);
    if (!series) {
        ui->regionInfoLabel->setText(
            QStringLiteral("<span style='color: red;'>Interval series \"%1\" not found</span>")
                .arg(QString::fromStdString(key)));
        ui->validationLabel->setText(QString{});

        bool was_valid = _valid;
        _valid = false;
        if (was_valid) {
            emit validityChanged(false);
        }
        return;
    }

    // --- Build info text ---
    size_t interval_count = series->size();

    // Compute total frame count across all intervals
    int64_t total_frames = 0;
    for (auto const & iwid : series->view()) {
        int64_t span = iwid.interval.end - iwid.interval.start + 1;
        if (span > 0) {
            total_frames += span;
        }
    }

    QString info = QStringLiteral("<b>Intervals:</b> %1<br>"
                                  "<b>Frames:</b> %2 across %1 intervals")
                       .arg(interval_count)
                       .arg(total_frames);

    // Time frame info
    TimeKey time_key = _data_manager->getTimeKey(key);
    if (!time_key.empty()) {
        auto time_frame = _data_manager->getTime(time_key);
        QString tf_info = QStringLiteral("<br><b>Time frame:</b> %1")
                              .arg(QString::fromStdString(time_key.str()));
        if (time_frame) {
            tf_info += QStringLiteral(" (%1 total frames)")
                           .arg(time_frame->getTotalFrameCount());
        }
        info += tf_info;
    }

    ui->regionInfoLabel->setText(info);

    // --- Validation ---
    bool new_valid = (interval_count > 0);
    QString validation_msg;

    if (interval_count == 0) {
        validation_msg = QStringLiteral(
            "<span style='color: orange;'>Warning: interval series has 0 intervals</span>");
    }

    ui->validationLabel->setText(validation_msg);

    if (new_valid != _valid) {
        _valid = new_valid;
        emit validityChanged(_valid);
    }
}

void RegionSelectionPanel::_registerDataManagerObserver() {
    if (!_data_manager) {
        return;
    }

    _dm_observer_id = _data_manager->addObserver([this]() {
        // Use a queued invocation to avoid re-entrancy during DataManager mutations
        QTimer::singleShot(0, this, &RegionSelectionPanel::refreshRegionList);
    });
}

void RegionSelectionPanel::_syncToState(std::string const & key) {
    if (!_state) {
        return;
    }

    if (_mode == RegionMode::Training) {
        _state->setTrainingRegionKey(key);
    } else {
        _state->setPredictionRegionKey(key);
    }
}

std::string RegionSelectionPanel::_readKeyFromState() const {
    if (!_state) {
        return {};
    }

    if (_mode == RegionMode::Training) {
        return _state->trainingRegionKey();
    }
    return _state->predictionRegionKey();
}
