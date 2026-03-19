#include "PredictionPanel.hpp"

#include "Core/MLCoreWidgetState.hpp"
#include "ui_PredictionPanel.h"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <QTimer>

#include <algorithm>

// =============================================================================
// Construction / destruction
// =============================================================================

PredictionPanel::PredictionPanel(
        std::shared_ptr<MLCoreWidgetState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PredictionPanel),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    ui->setupUi(this);
    _setupConnections();
    _registerDataManagerObserver();

    // Initial population
    refreshRegionList();

    // Restore from state
    _restoreFromState();
}

PredictionPanel::~PredictionPanel() {
    if (_dm_observer_id >= 0 && _data_manager) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

// =============================================================================
// Public API
// =============================================================================

bool PredictionPanel::hasValidConfiguration() const {
    return _valid;
}

std::string PredictionPanel::selectedRegionKey() const {
    if (ui->allFramesCheckBox->isChecked()) {
        return {};
    }
    int const idx = ui->regionComboBox->currentIndex();
    if (idx >= 0) {
        QVariant const data = ui->regionComboBox->itemData(idx);
        if (data.isValid()) {
            return data.toString().toStdString();
        }
    }
    return ui->regionComboBox->currentText().toStdString();
}

bool PredictionPanel::isAllFramesChecked() const {
    return ui->allFramesCheckBox->isChecked();
}

double PredictionPanel::threshold() const {
    return ui->thresholdSpinBox->value();
}

std::string PredictionPanel::outputPrefix() const {
    return ui->outputPrefixLineEdit->text().toStdString();
}

bool PredictionPanel::outputPredictions() const {
    return ui->outputPredictionsCheckBox->isChecked();
}

bool PredictionPanel::outputProbabilities() const {
    return ui->outputProbabilitiesCheckBox->isChecked();
}

// =============================================================================
// Public slots
// =============================================================================

void PredictionPanel::refreshRegionList() {
    if (!_data_manager) {
        return;
    }

    // Preserve current selection
    QString current_selection;
    int const current_idx = ui->regionComboBox->currentIndex();
    if (current_idx >= 0) {
        QVariant const data = ui->regionComboBox->itemData(current_idx);
        if (data.isValid()) {
            current_selection = data.toString();
        } else {
            current_selection = ui->regionComboBox->currentText();
        }
    }

    ui->regionComboBox->blockSignals(true);
    ui->regionComboBox->clear();

    // Empty sentinel item
    ui->regionComboBox->addItem(QString{});

    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    std::sort(interval_keys.begin(), interval_keys.end());

    for (auto const & key: interval_keys) {
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
    int restore_idx = -1;
    for (int i = 0; i < ui->regionComboBox->count(); ++i) {
        QVariant const data = ui->regionComboBox->itemData(i);
        if (data.isValid() && data.toString() == current_selection) {
            restore_idx = i;
            break;
        }
        if (ui->regionComboBox->itemText(i) == current_selection) {
            restore_idx = i;
            break;
        }
    }

    if (restore_idx != -1) {
        ui->regionComboBox->setCurrentIndex(restore_idx);
    } else {
        ui->regionComboBox->setCurrentIndex(0);
    }

    ui->regionComboBox->blockSignals(false);

    _updateRegionInfo();
}

// =============================================================================
// Private slots
// =============================================================================

void PredictionPanel::_onRegionComboChanged(int index) {
    QString key_str;
    if (index >= 0) {
        QVariant const data = ui->regionComboBox->itemData(index);
        if (data.isValid()) {
            key_str = data.toString();
        } else {
            key_str = ui->regionComboBox->itemText(index);
        }
    }

    std::string const key = key_str.toStdString();
    _syncRegionToState(key);
    _updateRegionInfo();
    emit regionChanged(key_str);
}

void PredictionPanel::_onAllFramesToggled(bool checked) {
    ui->regionComboBox->setEnabled(!checked);

    if (checked) {
        _syncRegionToState({});
        _updateRegionInfo();
        emit regionChanged(QString{});
    } else {
        // Re-enable and apply current combo selection
        _onRegionComboChanged(ui->regionComboBox->currentIndex());
    }
}

void PredictionPanel::_onThresholdSpinBoxChanged(double value) {
    // Sync slider (0–99 range maps to 0.01–0.99)
    ui->thresholdSlider->blockSignals(true);
    ui->thresholdSlider->setValue(static_cast<int>(value * 100.0));
    ui->thresholdSlider->blockSignals(false);

    if (!_updating && _state) {
        _state->setProbabilityThreshold(value);
    }
    emit thresholdChanged(value);
}

void PredictionPanel::_onThresholdSliderChanged(int value) {
    double const dval = static_cast<double>(value) / 100.0;

    // Sync spin box
    ui->thresholdSpinBox->blockSignals(true);
    ui->thresholdSpinBox->setValue(dval);
    ui->thresholdSpinBox->blockSignals(false);

    if (!_updating && _state) {
        _state->setProbabilityThreshold(dval);
    }
    emit thresholdChanged(dval);
}

void PredictionPanel::_onOutputPrefixChanged(QString const & text) {
    if (!_updating && _state) {
        _state->setOutputPrefix(text.toStdString());
    }
    _updateValidity();
    emit outputConfigChanged();
}

void PredictionPanel::_onOutputPredictionsToggled(bool checked) {
    if (!_updating && _state) {
        _state->setOutputPredictions(checked);
    }
    _updateValidity();
    emit outputConfigChanged();
}

void PredictionPanel::_onOutputProbabilitiesToggled(bool checked) {
    if (!_updating && _state) {
        _state->setOutputProbabilities(checked);
    }
    _updateValidity();
    emit outputConfigChanged();
}

void PredictionPanel::_onPredictClicked() {
    // Sync all current settings to state before emitting
    if (_state) {
        _state->setPredictionRegionKey(selectedRegionKey());
        _state->setOutputPrefix(outputPrefix());
        _state->setProbabilityThreshold(threshold());
        _state->setOutputPredictions(outputPredictions());
        _state->setOutputProbabilities(outputProbabilities());
    }
    emit predictRequested();
}

// =============================================================================
// Private helpers
// =============================================================================

void PredictionPanel::_setupConnections() {
    connect(ui->regionComboBox, &QComboBox::currentIndexChanged,
            this, &PredictionPanel::_onRegionComboChanged);

    connect(ui->refreshButton, &QPushButton::clicked,
            this, &PredictionPanel::refreshRegionList);

    connect(ui->allFramesCheckBox, &QCheckBox::toggled,
            this, &PredictionPanel::_onAllFramesToggled);

    connect(ui->thresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PredictionPanel::_onThresholdSpinBoxChanged);

    connect(ui->thresholdSlider, &QSlider::valueChanged,
            this, &PredictionPanel::_onThresholdSliderChanged);

    connect(ui->outputPrefixLineEdit, &QLineEdit::textChanged,
            this, &PredictionPanel::_onOutputPrefixChanged);

    connect(ui->outputPredictionsCheckBox, &QCheckBox::toggled,
            this, &PredictionPanel::_onOutputPredictionsToggled);

    connect(ui->outputProbabilitiesCheckBox, &QCheckBox::toggled,
            this, &PredictionPanel::_onOutputProbabilitiesToggled);

    connect(ui->predictButton, &QPushButton::clicked,
            this, &PredictionPanel::_onPredictClicked);

    // React to external state changes (e.g., from JSON restore)
    if (_state) {
        connect(_state.get(), &MLCoreWidgetState::predictionRegionKeyChanged,
                this, [this](QString const & key) {
                    if (_updating) {
                        return;
                    }
                    _updating = true;

                    if (key.isEmpty()) {
                        ui->allFramesCheckBox->setChecked(true);
                        ui->regionComboBox->setEnabled(false);
                    } else {
                        ui->allFramesCheckBox->setChecked(false);
                        ui->regionComboBox->setEnabled(true);

                        for (int i = 0; i < ui->regionComboBox->count(); ++i) {
                            QVariant const data = ui->regionComboBox->itemData(i);
                            if (data.isValid() && data.toString() == key) {
                                ui->regionComboBox->setCurrentIndex(i);
                                break;
                            }
                        }
                    }

                    _updating = false;
                    _updateRegionInfo();
                });

        connect(_state.get(), &MLCoreWidgetState::probabilityThresholdChanged,
                this, [this](double value) {
                    if (_updating) {
                        return;
                    }
                    _updating = true;
                    ui->thresholdSpinBox->setValue(value);
                    ui->thresholdSlider->setValue(static_cast<int>(value * 100.0));
                    _updating = false;
                });

        connect(_state.get(), &MLCoreWidgetState::outputPrefixChanged,
                this, [this](QString const & prefix) {
                    if (_updating) {
                        return;
                    }
                    _updating = true;
                    ui->outputPrefixLineEdit->setText(prefix);
                    _updating = false;
                });

        connect(_state.get(), &MLCoreWidgetState::outputProbabilitiesChanged,
                this, [this](bool enabled) {
                    if (_updating) {
                        return;
                    }
                    _updating = true;
                    ui->outputProbabilitiesCheckBox->setChecked(enabled);
                    _updating = false;
                    _updateValidity();
                });

        connect(_state.get(), &MLCoreWidgetState::outputPredictionsChanged,
                this, [this](bool enabled) {
                    if (_updating) {
                        return;
                    }
                    _updating = true;
                    ui->outputPredictionsCheckBox->setChecked(enabled);
                    _updating = false;
                    _updateValidity();
                });
    }
}

void PredictionPanel::_registerDataManagerObserver() {
    if (!_data_manager) {
        return;
    }

    _dm_observer_id = _data_manager->addObserver([this]() {
        QTimer::singleShot(0, this, &PredictionPanel::refreshRegionList);
    });
}

void PredictionPanel::_updateRegionInfo() {
    // "All remaining frames" mode
    if (ui->allFramesCheckBox->isChecked()) {
        ui->regionInfoLabel->setText(
                QStringLiteral("<b>All remaining frames</b> will be used for prediction"));
        _updateValidity();
        return;
    }

    std::string const key = selectedRegionKey();

    if (key.empty() || !_data_manager) {
        ui->regionInfoLabel->setText(QStringLiteral("No prediction region selected"));
        _updateValidity();
        return;
    }

    auto series = _data_manager->getData<DigitalIntervalSeries>(key);
    if (!series) {
        ui->regionInfoLabel->setText(
                QStringLiteral("<span style='color: red;'>Interval series \"%1\" not found</span>")
                        .arg(QString::fromStdString(key)));
        _updateValidity();
        return;
    }

    // Build info text
    size_t const interval_count = series->size();

    int64_t total_frames = 0;
    for (auto const & iwid: series->view()) {
        int64_t const span = iwid.interval.end - iwid.interval.start + 1;
        if (span > 0) {
            total_frames += span;
        }
    }

    QString info = QStringLiteral("<b>Intervals:</b> %1<br>"
                                  "<b>Frames:</b> %2 across %1 intervals")
                           .arg(interval_count)
                           .arg(total_frames);

    // Time frame info
    TimeKey const time_key = _data_manager->getTimeKey(key);
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
    _updateValidity();
}

void PredictionPanel::_updateValidity() {
    // Validity requires:
    // 1. A prediction region is configured (combo selection or "all frames")
    // 2. At least one output type is selected
    bool region_ok = ui->allFramesCheckBox->isChecked();
    if (!region_ok) {
        std::string const key = selectedRegionKey();
        if (!key.empty() && _data_manager) {
            auto series = _data_manager->getData<DigitalIntervalSeries>(key);
            region_ok = (series && series->size() > 0);
        }
    }

    bool const output_ok =
            ui->outputPredictionsCheckBox->isChecked() ||
            ui->outputProbabilitiesCheckBox->isChecked();

    bool const new_valid = region_ok && output_ok;

    // Update status label with helpful messages
    if (!region_ok && !output_ok) {
        ui->statusLabel->setText(
                QStringLiteral("<span style='color: orange;'>Select a prediction region "
                               "and at least one output type</span>"));
    } else if (!region_ok) {
        ui->statusLabel->setText(
                QStringLiteral("<span style='color: orange;'>Select a prediction region "
                               "or check \"All remaining frames\"</span>"));
    } else if (!output_ok) {
        ui->statusLabel->setText(
                QStringLiteral("<span style='color: orange;'>Select at least one output "
                               "type (predictions or probabilities)</span>"));
    } else {
        ui->statusLabel->setText(QString{});
    }

    if (new_valid != _valid) {
        _valid = new_valid;
        emit validityChanged(_valid);
    }
}

void PredictionPanel::_restoreFromState() {
    if (!_state) {
        return;
    }

    _updating = true;

    // Restore prediction region
    std::string const saved_key = _state->predictionRegionKey();
    if (saved_key.empty()) {
        ui->allFramesCheckBox->setChecked(true);
        ui->regionComboBox->setEnabled(false);
    } else {
        ui->allFramesCheckBox->setChecked(false);
        ui->regionComboBox->setEnabled(true);
        for (int i = 0; i < ui->regionComboBox->count(); ++i) {
            QVariant const data = ui->regionComboBox->itemData(i);
            if (data.isValid() && data.toString() == QString::fromStdString(saved_key)) {
                ui->regionComboBox->setCurrentIndex(i);
                break;
            }
        }
    }

    // Restore output prefix
    ui->outputPrefixLineEdit->setText(QString::fromStdString(_state->outputPrefix()));

    // Restore threshold
    double const saved_threshold = _state->probabilityThreshold();
    ui->thresholdSpinBox->setValue(saved_threshold);
    ui->thresholdSlider->setValue(static_cast<int>(saved_threshold * 100.0));

    // Restore output checkboxes
    ui->outputPredictionsCheckBox->setChecked(_state->outputPredictions());
    ui->outputProbabilitiesCheckBox->setChecked(_state->outputProbabilities());

    _updating = false;

    _updateRegionInfo();
}

void PredictionPanel::_syncRegionToState(std::string const & key) {
    if (!_updating && _state) {
        _state->setPredictionRegionKey(key);
    }
}
