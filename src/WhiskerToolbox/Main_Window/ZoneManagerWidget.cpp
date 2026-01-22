#include "ZoneManagerWidget.hpp"
#include "ZoneManager.hpp"
#include "ZoneConfig.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QDateTime>

ZoneManagerWidget::ZoneManagerWidget(ZoneManager * zone_manager, QWidget * parent)
    : QWidget(parent)
    , _zone_manager(zone_manager)
{
    setupUi();
    connectSignals();
    updateRatioDisplay();
}

void ZoneManagerWidget::setupUi() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // === File Operations Group ===
    auto * file_group = new QGroupBox(tr("Configuration File"));
    auto * file_layout = new QVBoxLayout(file_group);

    auto * path_layout = new QHBoxLayout();
    _config_path_edit = new QLineEdit();
    _config_path_edit->setPlaceholderText(tr("Path to zone_config.json"));
    path_layout->addWidget(_config_path_edit);

    auto * browse_btn = new QPushButton(tr("..."));
    browse_btn->setMaximumWidth(30);
    connect(browse_btn, &QPushButton::clicked, this, [this]() {
        QString const file_path = QFileDialog::getOpenFileName(
            this, tr("Select Zone Configuration"),
            QString(), tr("JSON Files (*.json);;All Files (*)"));
        if (!file_path.isEmpty()) {
            _config_path_edit->setText(file_path);
        }
    });
    path_layout->addWidget(browse_btn);
    file_layout->addLayout(path_layout);

    auto * btn_layout = new QHBoxLayout();
    _load_config_btn = new QPushButton(tr("Load Configuration"));
    _save_config_btn = new QPushButton(tr("Save Configuration"));
    btn_layout->addWidget(_load_config_btn);
    btn_layout->addWidget(_save_config_btn);
    file_layout->addLayout(btn_layout);

    main_layout->addWidget(file_group);

    // === Zone Ratios Group ===
    _ratios_group = new QGroupBox(tr("Zone Ratios"));
    auto * ratios_layout = new QGridLayout(_ratios_group);

    auto createRatioSpin = [](double initial_value) {
        auto * spin = new QDoubleSpinBox();
        spin->setRange(0.05, 0.90);
        spin->setSingleStep(0.01);
        spin->setDecimals(2);
        spin->setValue(initial_value);
        return spin;
    };

    ratios_layout->addWidget(new QLabel(tr("Left:")), 0, 0);
    _left_ratio_spin = createRatioSpin(0.15);
    ratios_layout->addWidget(_left_ratio_spin, 0, 1);

    ratios_layout->addWidget(new QLabel(tr("Center:")), 0, 2);
    _center_ratio_spin = createRatioSpin(0.70);
    ratios_layout->addWidget(_center_ratio_spin, 0, 3);

    ratios_layout->addWidget(new QLabel(tr("Right:")), 1, 0);
    _right_ratio_spin = createRatioSpin(0.15);
    ratios_layout->addWidget(_right_ratio_spin, 1, 1);

    ratios_layout->addWidget(new QLabel(tr("Bottom:")), 1, 2);
    _bottom_ratio_spin = createRatioSpin(0.10);
    ratios_layout->addWidget(_bottom_ratio_spin, 1, 3);

    _apply_ratios_btn = new QPushButton(tr("Apply Ratios"));
    ratios_layout->addWidget(_apply_ratios_btn, 2, 0, 1, 4);

    main_layout->addWidget(_ratios_group);

    // === Auto-save Group ===
    _autosave_group = new QGroupBox(tr("Auto-save"));
    auto * autosave_layout = new QVBoxLayout(_autosave_group);

    _autosave_enabled_check = new QCheckBox(tr("Enable auto-save on layout changes"));
    autosave_layout->addWidget(_autosave_enabled_check);

    auto * autosave_path_layout = new QHBoxLayout();
    autosave_path_layout->addWidget(new QLabel(tr("Save to:")));
    _autosave_path_edit = new QLineEdit();
    _autosave_path_edit->setPlaceholderText(tr("Auto-save file path"));
    autosave_path_layout->addWidget(_autosave_path_edit);

    _browse_autosave_btn = new QPushButton(tr("..."));
    _browse_autosave_btn->setMaximumWidth(30);
    connect(_browse_autosave_btn, &QPushButton::clicked, this, [this]() {
        QString const file_path = QFileDialog::getSaveFileName(
            this, tr("Select Auto-save Location"),
            QString(), tr("JSON Files (*.json);;All Files (*)"));
        if (!file_path.isEmpty()) {
            _autosave_path_edit->setText(file_path);
            onAutoSavePathChanged();
        }
    });
    autosave_path_layout->addWidget(_browse_autosave_btn);
    autosave_layout->addLayout(autosave_path_layout);

    main_layout->addWidget(_autosave_group);

    // === Status Log ===
    auto * log_group = new QGroupBox(tr("Status"));
    auto * log_layout = new QVBoxLayout(log_group);
    _log_text = new QTextEdit();
    _log_text->setReadOnly(true);
    _log_text->setMaximumHeight(100);
    _log_text->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 9pt;"));
    log_layout->addWidget(_log_text);

    main_layout->addWidget(log_group);

    // Add stretch to push everything up
    main_layout->addStretch();

    // Initialize auto-save state from ZoneManager
    _autosave_enabled_check->setChecked(_zone_manager->isAutoSaveEnabled());
    _autosave_path_edit->setText(_zone_manager->autoSaveFilePath());
}

void ZoneManagerWidget::connectSignals() {
    // Button connections
    connect(_load_config_btn, &QPushButton::clicked,
            this, &ZoneManagerWidget::onLoadConfigClicked);
    connect(_save_config_btn, &QPushButton::clicked,
            this, &ZoneManagerWidget::onSaveConfigClicked);
    connect(_apply_ratios_btn, &QPushButton::clicked,
            this, &ZoneManagerWidget::onApplyRatiosClicked);

    // Auto-save connections
    connect(_autosave_enabled_check, &QCheckBox::toggled,
            this, &ZoneManagerWidget::onAutoSaveToggled);
    connect(_autosave_path_edit, &QLineEdit::editingFinished,
            this, &ZoneManagerWidget::onAutoSavePathChanged);

    // ZoneManager signal connections
    connect(_zone_manager, &ZoneManager::zoneRatiosChanged,
            this, &ZoneManagerWidget::onZoneRatiosChanged);
    connect(_zone_manager, &ZoneManager::configLoaded,
            this, &ZoneManagerWidget::onConfigLoaded);
    connect(_zone_manager, &ZoneManager::configSaved,
            this, &ZoneManagerWidget::onConfigSaved);
    connect(_zone_manager, &ZoneManager::configLoadError,
            this, &ZoneManagerWidget::onConfigLoadError);
}

void ZoneManagerWidget::updateRatioDisplay() {
    auto const ratios = _zone_manager->currentRatios();

    // Block signals to prevent triggering apply
    _left_ratio_spin->blockSignals(true);
    _center_ratio_spin->blockSignals(true);
    _right_ratio_spin->blockSignals(true);
    _bottom_ratio_spin->blockSignals(true);

    _left_ratio_spin->setValue(static_cast<double>(ratios.left));
    _center_ratio_spin->setValue(static_cast<double>(ratios.center));
    _right_ratio_spin->setValue(static_cast<double>(ratios.right));
    _bottom_ratio_spin->setValue(static_cast<double>(ratios.bottom));

    _left_ratio_spin->blockSignals(false);
    _center_ratio_spin->blockSignals(false);
    _right_ratio_spin->blockSignals(false);
    _bottom_ratio_spin->blockSignals(false);
}

void ZoneManagerWidget::logMessage(QString const & message) {
    QString const timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    _log_text->append(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void ZoneManagerWidget::onLoadConfigClicked() {
    QString file_path = _config_path_edit->text();

    if (file_path.isEmpty()) {
        file_path = QFileDialog::getOpenFileName(
            this, tr("Load Zone Configuration"),
            QString(), tr("JSON Files (*.json);;All Files (*)"));

        if (file_path.isEmpty()) {
            return;  // User cancelled
        }
        _config_path_edit->setText(file_path);
    }

    logMessage(tr("Loading configuration from: %1").arg(file_path));

    QString const error = _zone_manager->loadConfigFromFile(file_path);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Load Error"),
                             tr("Failed to load configuration:\n%1").arg(error));
    }
}

void ZoneManagerWidget::onSaveConfigClicked() {
    QString file_path = _config_path_edit->text();

    if (file_path.isEmpty()) {
        file_path = QFileDialog::getSaveFileName(
            this, tr("Save Zone Configuration"),
            QStringLiteral("zone_config.json"),
            tr("JSON Files (*.json);;All Files (*)"));

        if (file_path.isEmpty()) {
            return;  // User cancelled
        }
        _config_path_edit->setText(file_path);
    }

    logMessage(tr("Saving configuration to: %1").arg(file_path));

    if (!_zone_manager->saveConfigToFile(file_path)) {
        QMessageBox::warning(this, tr("Save Error"),
                             tr("Failed to save configuration to:\n%1").arg(file_path));
    }
}

void ZoneManagerWidget::onApplyRatiosClicked() {
    float const left = static_cast<float>(_left_ratio_spin->value());
    float const center = static_cast<float>(_center_ratio_spin->value());
    float const right = static_cast<float>(_right_ratio_spin->value());
    float const bottom = static_cast<float>(_bottom_ratio_spin->value());

    // Normalize horizontal ratios
    float const h_sum = left + center + right;
    if (h_sum <= 0.0f) {
        logMessage(tr("Error: Invalid horizontal ratios"));
        return;
    }

    ZoneConfig::ZoneLayoutConfig config;
    config.zone_ratios.left = left / h_sum;
    config.zone_ratios.center = center / h_sum;
    config.zone_ratios.right = right / h_sum;
    config.zone_ratios.bottom = bottom;

    if (_zone_manager->applyConfig(config)) {
        logMessage(tr("Applied ratios: L=%.2f C=%.2f R=%.2f B=%.2f")
                       .arg(config.zone_ratios.left, 0, 'f', 2)
                       .arg(config.zone_ratios.center, 0, 'f', 2)
                       .arg(config.zone_ratios.right, 0, 'f', 2)
                       .arg(config.zone_ratios.bottom, 0, 'f', 2));
    } else {
        logMessage(tr("Error: Failed to apply ratios"));
    }
}

void ZoneManagerWidget::onAutoSaveToggled(bool enabled) {
    _zone_manager->setAutoSaveEnabled(enabled);
    _autosave_path_edit->setEnabled(enabled);
    _browse_autosave_btn->setEnabled(enabled);

    logMessage(enabled ? tr("Auto-save enabled") : tr("Auto-save disabled"));
}

void ZoneManagerWidget::onAutoSavePathChanged() {
    QString const path = _autosave_path_edit->text();
    _zone_manager->setAutoSaveFilePath(path);

    if (!path.isEmpty()) {
        logMessage(tr("Auto-save path set to: %1").arg(path));
    }
}

void ZoneManagerWidget::onZoneRatiosChanged() {
    updateRatioDisplay();
}

void ZoneManagerWidget::onConfigLoaded(QString const & file_path) {
    logMessage(tr("Configuration loaded from: %1").arg(file_path));
    updateRatioDisplay();
    emit configurationLoaded(file_path);
}

void ZoneManagerWidget::onConfigSaved(QString const & file_path) {
    logMessage(tr("Configuration saved to: %1").arg(file_path));
    emit configurationSaved(file_path);
}

void ZoneManagerWidget::onConfigLoadError(QString const & error) {
    logMessage(tr("Error: %1").arg(error));
}
