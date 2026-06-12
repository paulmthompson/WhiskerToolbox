/// @file StaticInputSlotWidget.cpp
/// @brief Implementation of StaticInputSlotWidget.

#include "StaticInputSlotWidget.hpp"

#include "DeepLearning_Widget/Core/BindingConversion.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning_Widget/Core/DeepLearningParamSchemasUIHints.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>

namespace dl::widget {

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

StaticInputSlotWidget::StaticInputSlotWidget(
        dl::TensorSlotDescriptor const & slot,
        std::shared_ptr<DataManager> dm,
        QWidget * parent)
    : QWidget(parent),
      _slot_name(slot.name),
      _recommended_encoder(slot.recommended_encoder),
      _dm(std::move(dm)) {

    assert(_dm && "StaticInputSlotWidget: DataManager must not be null");

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
    auto * shape_label = new QLabel(tr("Shape: %1").arg(shape_str), group);
    group_layout->addWidget(shape_label);

    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);

    auto schema =
            extractParameterSchema<StaticInputSlotParams>();
    _auto_param->setSchema(schema);

    _refreshSourceCombo();

    connect(_auto_param, &AutoParamWidget::parametersChanged, this,
            &StaticInputSlotWidget::bindingChanged);
}

StaticInputSlotWidget::~StaticInputSlotWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

StaticInputSlotParams StaticInputSlotWidget::params() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<StaticInputSlotParams>(json_str);
    if (result) {
        return result.value();
    }
    return {};
}

void StaticInputSlotWidget::setParams(StaticInputSlotParams const & p) {
    auto json = rfl::json::write(p);
    _auto_param->fromJson(json);
}

void StaticInputSlotWidget::refreshDataSources() {
    _refreshSourceCombo();
}

void StaticInputSlotWidget::refreshBankEntries(
        std::vector<std::string> const & bank_entry_ids) {
    _auto_param->updateAllowedValues("bank_entry_id", bank_entry_ids);
}

std::string const & StaticInputSlotWidget::slotName() const {
    return _slot_name;
}

StaticInputData StaticInputSlotWidget::toStaticInputData() const {
    return dl::conversion::fromStaticInputParams(_slot_name, params());
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void StaticInputSlotWidget::_refreshSourceCombo() {
    if (!_dm) return;

    auto const type_hint = SlotAssembler::dataTypeForEncoder(_recommended_encoder);
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    auto const keys = getKeysForTypes(*_dm, types);
    _auto_param->updateAllowedValues("data_key", keys);
}

}// namespace dl::widget
