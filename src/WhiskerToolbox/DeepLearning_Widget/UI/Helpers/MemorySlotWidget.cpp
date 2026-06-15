/// @file MemorySlotWidget.cpp
/// @brief Implementation of MemorySlotWidget.

#include "MemorySlotWidget.hpp"

#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/models_v2/TensorSlotDescriptor.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <algorithm>
#include <cassert>

namespace dl::widget {

namespace {

constexpr int kStaticKindIndex = 0;
constexpr int kRecurrentKindIndex = 1;

} // namespace

MemorySlotWidget::MemorySlotWidget(
        dl::TensorSlotDescriptor const & slot,
        std::shared_ptr<DataManager> dm,
        std::vector<std::string> output_slot_names,
        QWidget * parent)
    : QWidget(parent),
      _slot_name(slot.name),
      _recommended_encoder(slot.recommended_encoder),
      _dm(std::move(dm)),
      _output_slot_names(std::move(output_slot_names)) {

    assert(_dm && "MemorySlotWidget: DataManager must not be null");

    _seq_len = slot.hasSequenceDim()
                       ? slot.shape[static_cast<std::size_t>(slot.sequence_dim)]
                       : 1;

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto * group = new QGroupBox(
            QString::fromStdString(slot.name) + tr(" (memory)"), this);
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    auto * info = new QLabel(
            tr("Static: re-encode from DataManager or reuse a DataBank entry.\n"
               "Recurrent: feed back a model output from the previous frame."),
            group);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    group_layout->addWidget(info);

    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    group_layout->addWidget(new QLabel(tr("Shape: %1").arg(shape_str), group));

    if (slot.hasSequenceDim()) {
        group_layout->addWidget(new QLabel(
                tr("Sequence length: %1 (dim %2)")
                        .arg(_seq_len)
                        .arg(slot.sequence_dim),
                group));
    }

    _entries_container = new QWidget(group);
    auto * entries_layout = new QVBoxLayout(_entries_container);
    entries_layout->setContentsMargins(0, 0, 0, 0);
    entries_layout->setSpacing(4);
    group_layout->addWidget(_entries_container);

    if (_seq_len > 1) {
        auto * btn_row = new QHBoxLayout();
        _add_btn = new QPushButton(tr("+ Add Entry"), group);
        _add_btn->setToolTip(
                tr("Add another sequence position (up to %1)").arg(_seq_len));
        _remove_btn = new QPushButton(tr("- Remove Last"), group);
        _remove_btn->setToolTip(tr("Remove the last sequence entry"));
        btn_row->addWidget(_add_btn);
        btn_row->addWidget(_remove_btn);
        btn_row->addStretch();
        group_layout->addLayout(btn_row);

        connect(_add_btn, &QPushButton::clicked, this, [this] {
            if (static_cast<int>(_entry_rows.size()) >= static_cast<int>(_seq_len)) {
                return;
            }
            _addEntryRow(static_cast<int>(_entry_rows.size()));
            emit bindingChanged();
        });

        connect(_remove_btn, &QPushButton::clicked, this, [this] {
            if (_entry_rows.size() <= 1) return;
            _removeLastEntry();
            emit bindingChanged();
        });
    }

    _addEntryRow(0);
}

MemorySlotWidget::~MemorySlotWidget() = default;

void MemorySlotWidget::_addEntryRow(int memory_index) {
    EntryRow row;
    row.memory_index = memory_index;

    row.group = new QGroupBox(
            _seq_len > 1 ? tr("Position %1").arg(memory_index) : QString{},
            _entries_container);
    auto * row_layout = new QVBoxLayout(row.group);

    auto * kind_row = new QHBoxLayout();
    kind_row->addWidget(new QLabel(tr("Frame kind:"), row.group));
    row.kind_combo = new QComboBox(row.group);
    row.kind_combo->addItem(tr("Static"));
    row.kind_combo->addItem(tr("Recurrent"));
    kind_row->addWidget(row.kind_combo, 1);
    row_layout->addLayout(kind_row);

    row.stack = new QStackedWidget(row.group);
    row_layout->addWidget(row.stack);

    row.static_param = new AutoParamWidget(row.stack);
    row.static_param->setSchema(extractParameterSchema<StaticFrameSourceForm>());
    row.stack->addWidget(row.static_param);

    row.recurrent_param = new AutoParamWidget(row.stack);
    row.recurrent_param->setSchema(extractParameterSchema<RecurrentFrameSourceForm>());
    row.stack->addWidget(row.recurrent_param);

    if (auto * entries_layout =
                qobject_cast<QVBoxLayout *>(_entries_container->layout())) {
        entries_layout->addWidget(row.group);
    }

    auto const row_index = static_cast<int>(_entry_rows.size());
    _entry_rows.push_back(row);

    connect(_entry_rows.back().kind_combo, &QComboBox::currentIndexChanged, this,
            [this, row_index](int) {
                if (row_index < 0 ||
                    row_index >= static_cast<int>(_entry_rows.size())) {
                    return;
                }
                _onEntryKindChanged(_entry_rows[static_cast<std::size_t>(row_index)]);
            });

    connect(_entry_rows.back().static_param, &AutoParamWidget::parametersChanged,
            this, &MemorySlotWidget::bindingChanged);
    connect(_entry_rows.back().recurrent_param, &AutoParamWidget::parametersChanged,
            this, &MemorySlotWidget::bindingChanged);

    _refreshDataKeyCombos();
    _refreshOutputSlotCombos();
}

void MemorySlotWidget::_removeLastEntry() {
    if (_entry_rows.size() <= 1) return;
    auto & last = _entry_rows.back();
    if (last.group) {
        last.group->deleteLater();
    }
    _entry_rows.pop_back();
}

void MemorySlotWidget::_onEntryKindChanged(EntryRow & row) {
    row.stack->setCurrentIndex(row.kind_combo->currentIndex());
}

std::string const & MemorySlotWidget::slotName() const {
    return _slot_name;
}

std::vector<MemoryFrameBinding> MemorySlotWidget::bindings() const {
    std::vector<MemoryFrameBinding> result;
    for (auto const & row: _entry_rows) {
        auto binding = _bindingFromRow(row);
        if (dl::isStaticFrame(binding) && !dl::hasActiveStaticSource(binding)) {
            continue;
        }
        if (dl::isRecurrentFrame(binding) &&
            !dl::hasActiveRecurrentBinding(binding)) {
            continue;
        }
        result.push_back(std::move(binding));
    }
    return result;
}

void MemorySlotWidget::setBindings(std::vector<MemoryFrameBinding> const & all_frames) {
    std::vector<MemoryFrameBinding> slot_frames;
    for (auto const & frame: all_frames) {
        if (frame.slot_name == _slot_name) {
            slot_frames.push_back(frame);
        }
    }

    int max_idx = 0;
    for (auto const & frame: slot_frames) {
        max_idx = std::max(max_idx, frame.memory_index + 1);
    }
    if (max_idx == 0) {
        max_idx = 1;
    }

    while (static_cast<int>(_entry_rows.size()) < max_idx) {
        _addEntryRow(static_cast<int>(_entry_rows.size()));
    }

    for (auto & row: _entry_rows) {
        MemoryFrameBinding binding;
        binding.slot_name = _slot_name;
        binding.memory_index = row.memory_index;
        binding.frame = StaticFrameSource{};

        for (auto const & saved: slot_frames) {
            if (saved.memory_index == row.memory_index) {
                binding = saved;
                break;
            }
        }

        _setRowFromBinding(row, binding);
    }

    _refreshDataKeyCombos();
    _refreshOutputSlotCombos();
}

void MemorySlotWidget::refreshDataSources() {
    _refreshDataKeyCombos();
}

void MemorySlotWidget::refreshBankEntries(
        std::vector<std::string> const & bank_entry_ids) {
    for (auto & row: _entry_rows) {
        row.static_param->updateAllowedValues("bank_entry_id", bank_entry_ids);
    }
}

void MemorySlotWidget::refreshOutputSlots(
        std::vector<std::string> const & output_slot_names) {
    _output_slot_names = output_slot_names;
    _refreshOutputSlotCombos();
}

MemoryFrameBinding MemorySlotWidget::_bindingFromRow(EntryRow const & row) const {
    if (row.kind_combo->currentIndex() == kRecurrentKindIndex) {
        auto json_str = row.recurrent_param->toJson();
        auto parsed = rfl::json::read<RecurrentFrameSourceForm>(json_str);
        if (!parsed) {
            return MemoryFrameBinding{.slot_name = _slot_name,
                                      .memory_index = row.memory_index};
        }
        return toRecurrentMemoryFrame(_slot_name, row.memory_index, parsed.value());
    }

    auto json_str = row.static_param->toJson();
    auto parsed = rfl::json::read<StaticFrameSourceForm>(json_str);
    if (!parsed) {
        return MemoryFrameBinding{.slot_name = _slot_name,
                                  .memory_index = row.memory_index};
    }
    return toStaticMemoryFrame(_slot_name, row.memory_index, parsed.value());
}

void MemorySlotWidget::_setRowFromBinding(
        EntryRow & row,
        MemoryFrameBinding const & binding) {
    QSignalBlocker const kind_blocker(row.kind_combo);
    if (dl::isRecurrentFrame(binding)) {
        row.kind_combo->setCurrentIndex(kRecurrentKindIndex);
        row.stack->setCurrentIndex(kRecurrentKindIndex);
        auto form = fromRecurrentMemoryFrame(binding);
        row.recurrent_param->fromJson(rfl::json::write(form));
    } else {
        row.kind_combo->setCurrentIndex(kStaticKindIndex);
        row.stack->setCurrentIndex(kStaticKindIndex);
        auto form = fromStaticMemoryFrame(binding);
        row.static_param->fromJson(rfl::json::write(form));
    }
}

void MemorySlotWidget::_refreshDataKeyCombos() {
    if (!_dm) return;
    auto const type_hint =
            SlotAssembler::dataTypeForEncoder(_recommended_encoder);
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    auto const keys = getKeysForTypes(*_dm, types);

    for (auto & row: _entry_rows) {
        row.static_param->updateAllowedValues("data_key", keys);
        row.recurrent_param->updateAllowedValues("data_key", keys);
    }
}

void MemorySlotWidget::_refreshOutputSlotCombos() {
    for (auto & row: _entry_rows) {
        row.recurrent_param->updateAllowedValues("output_slot_name",
                                                 _output_slot_names);
    }
}

} // namespace dl::widget
