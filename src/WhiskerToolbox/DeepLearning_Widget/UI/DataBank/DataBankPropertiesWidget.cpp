/**
 * @file DataBankPropertiesWidget.cpp
 * @brief Implementation of the memory bank properties panel section.
 */

#include "DataBankPropertiesWidget.hpp"

#include "ui_DataBankPropertiesWidget.h"

#include "DeepLearning/bindings/BindingParamSchemas.hpp"
#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning/storage/DataBank.hpp"
#include "Media/Media_Data.hpp"

#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

#include <algorithm>
#include <cassert>

namespace dl::widget {

namespace {

[[nodiscard]] ImageSize resolveSourceImageSize(DataManager & dm) {
    for (auto const & key: dm.getAllKeys()) {
        if (auto media = dm.getData<MediaData>(key)) {
            return media->getImageSize();
        }
    }
    return ImageSize{256, 256};
}

[[nodiscard]] std::string comboDataKey(QComboBox const * combo) {
    if (!combo) {
        return {};
    }
    auto const text = combo->currentText();
    if (text.isEmpty() || text == QStringLiteral("(None)")) {
        return {};
    }
    return text.toStdString();
}

}// namespace

DataBankPropertiesWidget::DataBankPropertiesWidget(QWidget * parent)
    : QWidget(parent),
      _ui(std::make_unique<Ui::DataBankPropertiesWidget>()) {
    _ui->setupUi(this);

    connect(_ui->targetSlotCombo, &QComboBox::currentIndexChanged,
            this, &DataBankPropertiesWidget::_onSlotSelectionChanged);
    connect(_ui->memoryIndexSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DataBankPropertiesWidget::_onMemoryIndexChanged);
    connect(_ui->captureButton, &QPushButton::clicked,
            this, &DataBankPropertiesWidget::_onCaptureClicked);
    connect(_ui->dataSourceCombo, &QComboBox::currentIndexChanged,
            this, [this]() { _updateCaptureEnabled(); });

    _updateMemoryIndexVisibility();
    _updateEntryIdLabel();
    _updateCaptureEnabled();
}

DataBankPropertiesWidget::~DataBankPropertiesWidget() = default;

void DataBankPropertiesWidget::setAssembler(SlotAssembler * assembler) {
    _assembler = assembler;
    refreshCaptureStatus();
    _updateCaptureEnabled();
}

void DataBankPropertiesWidget::setDataManager(std::shared_ptr<DataManager> dm) {
    assert(dm && "DataBankPropertiesWidget: DataManager must not be null");
    _dm = std::move(dm);
    _combo_helper = std::make_unique<DataSourceComboHelper>(_dm, this);
    refreshDataSources();
}

void DataBankPropertiesWidget::setStaticSlots(
        std::vector<dl::TensorSlotDescriptor> const & static_slot_list) {
    _static_slots = static_slot_list;

    auto const previous = _ui->targetSlotCombo->currentText();
    _ui->targetSlotCombo->clear();

    for (auto const & slot: _static_slots) {
        _ui->targetSlotCombo->addItem(QString::fromStdString(slot.name));
    }

    if (!previous.isEmpty()) {
        int const idx = _ui->targetSlotCombo->findText(previous);
        if (idx >= 0) {
            _ui->targetSlotCombo->setCurrentIndex(idx);
        }
    }

    _onSlotSelectionChanged();
}

void DataBankPropertiesWidget::setModelReady(bool ready) {
    _model_ready = ready;
    _updateCaptureEnabled();
}

void DataBankPropertiesWidget::setCurrentFrame(int frame) {
    _current_frame = frame;
}

void DataBankPropertiesWidget::refreshDataSources() {
    _refreshDataSourceCombo();
    _updateCaptureEnabled();
}

void DataBankPropertiesWidget::refreshCaptureStatus() {
    if (!_assembler) {
        _setCaptureStatus(false, -1, {0.0f, 0.0f});
        return;
    }

    auto const entry_id = _resolvedEntryId();
    if (entry_id.empty()) {
        _setCaptureStatus(false, -1, {0.0f, 0.0f});
        return;
    }

    auto const & bank = _assembler->dataBank();
    if (!bank.getEncoded(entry_id)) {
        _setCaptureStatus(false, -1, {0.0f, 0.0f});
        return;
    }

    int captured_frame = -1;
    if (auto const entry = bank.get(entry_id)) {
        captured_frame = entry->metadata.captured_frame;
    }

    auto const range = bank.encodedRange(entry_id);
    _setCaptureStatus(true, captured_frame, range);
}

void DataBankPropertiesWidget::_onSlotSelectionChanged() {
    _updateMemoryIndexVisibility();
    _updateEntryIdLabel();
    _refreshDataSourceCombo();
    refreshCaptureStatus();
    _updateCaptureEnabled();
}

void DataBankPropertiesWidget::_onMemoryIndexChanged() {
    _updateEntryIdLabel();
    refreshCaptureStatus();
}

void DataBankPropertiesWidget::_onCaptureClicked() {
    if (!_assembler || !_dm) {
        return;
    }
    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    auto const * slot = _selectedSlot();
    if (!slot) {
        QMessageBox::warning(this, tr("No Target Slot"),
                             tr("Select a static input slot first."));
        return;
    }

    auto const data_key = comboDataKey(_ui->dataSourceCombo);
    if (data_key.empty()) {
        QMessageBox::warning(this, tr("No Source"),
                             tr("Select a data source to capture."));
        return;
    }

    auto const entry_id = _resolvedEntryId();
    if (entry_id.empty()) {
        return;
    }

    auto entry = dl::makeCaptureBinding(slot->name, _memoryIndex(), data_key);

    auto const source_size = resolveSourceImageSize(*_dm);
    bool const ok = _assembler->captureToBank(
            *_dm, entry_id, entry, _current_frame, source_size);

    if (ok) {
        auto const range = _assembler->dataBank().encodedRange(entry_id);
        _setCaptureStatus(true, _current_frame, range);
        emit dataBankChanged();
    } else {
        _setCaptureStatus(false, -1, {0.0f, 0.0f});
        QMessageBox::warning(this, tr("Capture Failed"),
                             tr("Failed to capture frame %1 into entry '%2'.")
                                     .arg(_current_frame)
                                     .arg(QString::fromStdString(entry_id)));
    }
}

dl::TensorSlotDescriptor const *
DataBankPropertiesWidget::_selectedSlot() const {
    if (_static_slots.empty()) {
        return nullptr;
    }

    auto const slot_name = _ui->targetSlotCombo->currentText().toStdString();
    for (auto const & slot: _static_slots) {
        if (slot.name == slot_name) {
            return &slot;
        }
    }
    return nullptr;
}

int DataBankPropertiesWidget::_memoryIndex() const {
    return _ui->memoryIndexSpin->value();
}

std::string DataBankPropertiesWidget::_resolvedEntryId() const {
    auto const * slot = _selectedSlot();
    if (!slot) {
        return {};
    }
    return defaultBankEntryId(slot->name, _memoryIndex());
}

void DataBankPropertiesWidget::_updateMemoryIndexVisibility() {
    auto const * slot = _selectedSlot();
    bool const show_index = slot && slot->hasSequenceDim();
    _ui->memoryIndexLabel->setVisible(show_index);
    _ui->memoryIndexSpin->setVisible(show_index);

    if (show_index) {
        int const seq_len = static_cast<int>(
                slot->shape[static_cast<std::size_t>(slot->sequence_dim)]);
        int const max_index = std::max(0, seq_len - 1);
        _ui->memoryIndexSpin->setRange(0, max_index);
        if (_ui->memoryIndexSpin->value() > max_index) {
            _ui->memoryIndexSpin->setValue(0);
        }
    } else {
        _ui->memoryIndexSpin->setValue(0);
    }
}

void DataBankPropertiesWidget::_updateEntryIdLabel() {
    auto const entry_id = _resolvedEntryId();
    if (entry_id.empty()) {
        _ui->entryIdLabel->setText(QStringLiteral("—"));
    } else {
        _ui->entryIdLabel->setText(QString::fromStdString(entry_id));
    }
}

void DataBankPropertiesWidget::_refreshDataSourceCombo() {
    if (!_combo_helper) {
        return;
    }

    auto const * slot = _selectedSlot();
    if (!slot) {
        _ui->dataSourceCombo->clear();
        return;
    }

    auto const type_hint = SlotAssembler::dataTypeForEncoder(
            slot->recommended_encoder);
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    _combo_helper->populateCombo(_ui->dataSourceCombo, types);
}

void DataBankPropertiesWidget::_updateCaptureEnabled() {
    bool const can_capture = _model_ready &&
                             _assembler != nullptr &&
                             _selectedSlot() != nullptr &&
                             !comboDataKey(_ui->dataSourceCombo).empty();
    _ui->captureButton->setEnabled(can_capture);
}

void DataBankPropertiesWidget::_setCaptureStatus(
        bool captured,
        int captured_frame,
        std::pair<float, float> value_range) {
    if (!captured) {
        _ui->captureStatusLabel->setText(tr("Not captured"));
        _ui->captureStatusLabel->setStyleSheet(
                QStringLiteral("color: gray; font-size: 10px;"));
        return;
    }

    QString info = tr("\u2713 Captured");
    if (captured_frame >= 0) {
        info += tr(" (frame %1)").arg(captured_frame);
    }
    info += tr(" range [%1, %2]")
                    .arg(static_cast<double>(value_range.first), 0, 'f', 2)
                    .arg(static_cast<double>(value_range.second), 0, 'f', 2);
    _ui->captureStatusLabel->setText(info);
    _ui->captureStatusLabel->setStyleSheet(
            QStringLiteral("color: green; font-size: 10px;"));
}

}// namespace dl::widget
