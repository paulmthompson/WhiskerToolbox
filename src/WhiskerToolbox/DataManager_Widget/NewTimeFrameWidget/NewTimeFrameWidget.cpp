/**
 * @file NewTimeFrameWidget.cpp
 * @brief Implementation of NewTimeFrameWidget for creating upsampled TimeFrames.
 */

#include "NewTimeFrameWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "ui_NewTimeFrameWidget.h"

#include <QSpinBox>

NewTimeFrameWidget::NewTimeFrameWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::NewTimeFrameWidget) {
    ui->setupUi(this);

    connect(ui->create_timeframe_button, &QPushButton::clicked,
            this, &NewTimeFrameWidget::_createTimeFrame);

    connect(ui->source_timeframe_combo, &QComboBox::currentIndexChanged,
            this, [this](int) {
                _updatePreview();
                _autoSuggestName();
            });

    connect(ui->upsampling_factor_spin, &QSpinBox::valueChanged,
            this, [this](int) {
                _updatePreview();
                _autoSuggestName();
            });
}

NewTimeFrameWidget::~NewTimeFrameWidget() {
    delete ui;
}

void NewTimeFrameWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    populateTimeframes();
}

void NewTimeFrameWidget::populateTimeframes() {
    ui->source_timeframe_combo->clear();

    if (!_data_manager) {
        return;
    }

    auto timeframe_keys = _data_manager->getTimeFrameKeys();

    for (auto const & key: timeframe_keys) {
        ui->source_timeframe_combo->addItem(QString::fromStdString(key.str()));
    }

    if (ui->source_timeframe_combo->count() > 0) {
        // Default to "time" if it exists
        int const time_index = ui->source_timeframe_combo->findText("time");
        if (time_index >= 0) {
            ui->source_timeframe_combo->setCurrentIndex(time_index);
        }
    }

    _updatePreview();
    _autoSuggestName();
}

void NewTimeFrameWidget::_updatePreview() {
    if (!_data_manager || ui->source_timeframe_combo->count() == 0) {
        ui->preview_label->setText("No source selected");
        return;
    }

    auto const source_key = ui->source_timeframe_combo->currentText().toStdString();
    auto source_tf = _data_manager->getTime(TimeKey(source_key));
    if (!source_tf) {
        ui->preview_label->setText("Source not found");
        return;
    }

    auto const n = static_cast<int64_t>(source_tf->getTotalFrameCount());
    int const factor = ui->upsampling_factor_spin->value();

    if (n <= 1) {
        ui->preview_label->setText("Cannot upsample (too few entries)");
        return;
    }

    auto const output_count = (n - 1) * factor + 1;
    ui->preview_label->setText(
            QString("%1 entries → %2 entries").arg(n).arg(output_count));
}

void NewTimeFrameWidget::_autoSuggestName() {
    // Only auto-suggest if the name field is empty or still matches the last auto-suggestion
    auto const current_name = ui->new_timeframe_name->text();
    if (!current_name.isEmpty() && current_name != _last_auto_name) {
        return;
    }

    if (ui->source_timeframe_combo->count() == 0) {
        return;
    }

    auto const source_name = ui->source_timeframe_combo->currentText();
    int const factor = ui->upsampling_factor_spin->value();
    auto const suggested = QString("%1_%2x").arg(source_name).arg(factor);

    _last_auto_name = suggested;
    ui->new_timeframe_name->setText(suggested);
}

void NewTimeFrameWidget::_createTimeFrame() {
    auto const name = ui->new_timeframe_name->text().trimmed().toStdString();
    if (name.empty()) {
        ui->status_label->setText("Name cannot be empty");
        return;
    }

    if (!_data_manager) {
        return;
    }

    // Check for duplicate name
    auto const existing_keys = _data_manager->getTimeFrameKeys();
    for (auto const & key: existing_keys) {
        if (key.str() == name) {
            ui->status_label->setText("A TimeFrame with this name already exists");
            return;
        }
    }

    auto const source_key = ui->source_timeframe_combo->currentText().toStdString();
    int const factor = ui->upsampling_factor_spin->value();

    ui->status_label->clear();
    emit createTimeFrame(name, source_key, factor);
}
