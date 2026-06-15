/// @file SequenceSlotWidget.cpp
/// @brief Implementation of SequenceSlotWidget.

#include "SequenceSlotWidget.hpp"

#include "DeepLearning_Widget/Core/BindingConversion.hpp"
#include "DeepLearning_Widget/Core/DeepLearningParamSchemasUIHints.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/models_v2/TensorSlotDescriptor.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <algorithm>
#include <cassert>

namespace dl::widget {

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

SequenceSlotWidget::SequenceSlotWidget(
        dl::TensorSlotDescriptor const & slot,
        std::shared_ptr<DataManager> dm,
        std::vector<std::string> output_slot_names,
        QWidget * parent)
    : QWidget(parent),
      _slot_name(slot.name),
      _recommended_encoder(slot.recommended_encoder),
      _dm(std::move(dm)),
      _output_slot_names(std::move(output_slot_names)) {

    assert(_dm && "SequenceSlotWidget: DataManager must not be null");

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
            tr("DataManager: re-encodes each run at current_frame + offset.\n"
               "DataBank: reuses a pre-captured entry from the Memory Bank."),
            group);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    group_layout->addWidget(info);

    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    group_layout->addWidget(
            new QLabel(tr("Shape: %1").arg(shape_str), group));

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
    });

    connect(_remove_btn, &QPushButton::clicked, this, [this] {
        if (_entry_rows.size() <= 1) return;
        _removeLastEntry();
    });

    _addEntryRow(0);
}

SequenceSlotWidget::~SequenceSlotWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Entry row management
// ════════════════════════════════════════════════════════════════════════════

void SequenceSlotWidget::_addEntryRow(int memory_index) {
    EntryRow row;
    row.memory_index = memory_index;

    row.group = new QGroupBox(tr("Position %1").arg(memory_index),
                              _entries_container);
    auto * row_layout = new QVBoxLayout(row.group);

    row.auto_param = new AutoParamWidget(row.group);
    auto schema = extractParameterSchema<
            SequenceEntryParams>();
    row.auto_param->setSchema(schema);
    row_layout->addWidget(row.auto_param);

    auto * entries_layout =
            qobject_cast<QVBoxLayout *>(_entries_container->layout());
    if (entries_layout) {
        entries_layout->addWidget(row.group);
    }

    connect(row.auto_param, &AutoParamWidget::parametersChanged, this,
            &SequenceSlotWidget::bindingChanged);

    _refreshDataKeyCombos();
    _refreshOutputSlotCombos();

    _entry_rows.push_back(std::move(row));
}

void SequenceSlotWidget::_removeLastEntry() {
    if (_entry_rows.size() <= 1) return;
    auto & last = _entry_rows.back();
    if (last.group) {
        last.group->deleteLater();
    }
    _entry_rows.pop_back();
}

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

std::string const & SequenceSlotWidget::slotName() const {
    return _slot_name;
}

void SequenceSlotWidget::refreshDataSources() {
    _refreshDataKeyCombos();
}

void SequenceSlotWidget::refreshBankEntries(
        std::vector<std::string> const & bank_entry_ids) {
    for (auto & row: _entry_rows) {
        row.auto_param->updateAllowedValues("bank_entry_id", bank_entry_ids);
    }
}

void SequenceSlotWidget::refreshOutputSlots(
        std::vector<std::string> const & output_slot_names) {
    _output_slot_names = output_slot_names;
    _refreshOutputSlotCombos();
}

std::vector<StaticInputData> SequenceSlotWidget::getStaticInputs() const {
    std::vector<StaticInputData> result;
    for (auto const & row: _entry_rows) {
        auto json_str = row.auto_param->toJson();
        auto parsed = rfl::json::read<SequenceEntryParams>(json_str);
        if (!parsed) continue;

        parsed.value().entry.visit([&](auto const & v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, StaticSequenceEntryParams>) {
                auto entry = dl::conversion::fromStaticSequenceEntryParams(
                        _slot_name, row.memory_index, v);
                if (!entry.hasActiveSource()) {
                    return;
                }
                result.push_back(std::move(entry));
            }
        });
    }
    return result;
}

std::vector<RecurrentBindingData>
SequenceSlotWidget::getRecurrentBindings() const {
    std::vector<RecurrentBindingData> result;
    for (auto const & row: _entry_rows) {
        auto json_str = row.auto_param->toJson();
        auto parsed = rfl::json::read<SequenceEntryParams>(json_str);
        if (!parsed) continue;

        parsed.value().entry.visit([&](auto const & v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T,
                                         RecurrentSequenceEntryParams>) {
                if (v.output_slot_name.empty() ||
                    v.output_slot_name == "(None)") {
                    return;
                }

                result.push_back(
                        dl::conversion::fromRecurrentSequenceEntryParams(
                                _slot_name, row.memory_index, v));
            }
        });
    }
    return result;
}

void SequenceSlotWidget::setEntriesFromState(
        std::vector<StaticInputData> const & static_inputs,
        std::vector<RecurrentBindingData> const & recurrent_bindings) {
    int max_idx = 0;
    for (auto const & si: static_inputs) {
        if (si.slot_name == _slot_name) {
            max_idx = std::max(max_idx, si.memory_index + 1);
        }
    }
    for (auto const & rb: recurrent_bindings) {
        if (rb.input_slot_name == _slot_name && rb.target_memory_index >= 0) {
            max_idx = std::max(max_idx, rb.target_memory_index + 1);
        }
    }
    if (max_idx == 0) max_idx = 1;

    while (static_cast<int>(_entry_rows.size()) < max_idx) {
        _addEntryRow(static_cast<int>(_entry_rows.size()));
    }

    _refreshDataKeyCombos();
    _refreshOutputSlotCombos();

    for (auto & row: _entry_rows) {
        SequenceEntryVariant params = StaticSequenceEntryParams{};

        bool found_static = false;
        for (auto const & si: static_inputs) {
            if (si.slot_name == _slot_name &&
                si.memory_index == row.memory_index) {
                params = dl::conversion::toStaticSequenceEntryParams(si);
                found_static = true;
                break;
            }
        }

        if (!found_static) {
            for (auto const & rb: recurrent_bindings) {
                if (rb.input_slot_name == _slot_name &&
                    rb.target_memory_index == row.memory_index) {
                    params = RecurrentSequenceEntryParams{
                            .output_slot_name = rb.output_slot_name,
                            .init_mode_str = rb.init_mode_str};
                    break;
                }
            }
        }

        SequenceEntryParams const holder{.entry = params};
        auto json = rfl::json::write(holder);
        row.auto_param->fromJson(json);
    }

    _refreshDataKeyCombos();
    _refreshOutputSlotCombos();
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void SequenceSlotWidget::_refreshDataKeyCombos() {
    if (!_dm) return;
    auto const type_hint =
            SlotAssembler::dataTypeForEncoder(_recommended_encoder);
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    auto const keys = getKeysForTypes(*_dm, types);

    for (auto & row: _entry_rows) {
        row.auto_param->updateAllowedValues("data_key", keys);
    }
}

void SequenceSlotWidget::_refreshOutputSlotCombos() {
    for (auto & row: _entry_rows) {
        row.auto_param->updateAllowedValues("output_slot_name",
                                            _output_slot_names);
    }
}

}// namespace dl::widget
