/// @file RecurrentBindingWidget.cpp
/// @brief Implementation of RecurrentBindingWidget.

#include "RecurrentBindingWidget.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>
#include <type_traits>

namespace dl::widget {

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

RecurrentBindingWidget::RecurrentBindingWidget(
        dl::TensorSlotDescriptor const & input_slot,
        std::vector<dl::TensorSlotDescriptor> const & output_slots,
        std::shared_ptr<DataManager> dm,
        QWidget * parent)
    : QWidget(parent),
      _input_slot_name(input_slot.name),
      _dm(std::move(dm)),
      _recommended_encoder(input_slot.recommended_encoder) {

    assert(_dm && "RecurrentBindingWidget: DataManager must not be null");

    for (auto const & out: output_slots) {
        _output_slot_names.push_back(out.name);
    }

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto * group = new QGroupBox(
            QString::fromStdString(input_slot.name) + tr(" (recurrent)"), this);
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    auto * info = new QLabel(
            tr("Map a model output to this input slot.\n"
               "The output at frame t becomes the input at frame t+1."),
            group);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    group_layout->addWidget(info);

    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);

    auto schema = WhiskerToolbox::Transforms::V2::extractParameterSchema<
            RecurrentBindingSlotParams>();
    _auto_param->setSchema(schema);

    _refreshOutputSlotCombo();
    _refreshDataKeyCombo();

    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        _refreshDataKeyCombo();
        emit bindingChanged();
    });
}

RecurrentBindingWidget::~RecurrentBindingWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

RecurrentBindingSlotParams RecurrentBindingWidget::params() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<RecurrentBindingSlotParams>(json_str);
    if (result) {
        return result.value();
    }
    return {};
}

void RecurrentBindingWidget::setParams(RecurrentBindingSlotParams const & p) {
    auto json = rfl::json::write(p);
    _auto_param->fromJson(json);
}

void RecurrentBindingWidget::refreshOutputSlots(
        std::vector<std::string> const & output_names) {
    _output_slot_names = output_names;
    _refreshOutputSlotCombo();
}

void RecurrentBindingWidget::refreshDataSources() {
    _refreshDataKeyCombo();
}

std::string const & RecurrentBindingWidget::slotName() const {
    return _input_slot_name;
}

RecurrentBindingData RecurrentBindingWidget::toRecurrentBindingData() const {
    auto const p = params();
    RecurrentBindingData binding;
    binding.input_slot_name = _input_slot_name;
    binding.output_slot_name =
            (p.output_slot_name == "(None)") ? "" : p.output_slot_name;
    binding.target_memory_index = -1;

    p.init.visit([&](auto const & init) {
        using T = std::decay_t<decltype(init)>;
        if constexpr (std::is_same_v<T, ZerosInitParams>) {
            binding.init_mode_str = "Zeros";
        } else if constexpr (std::is_same_v<T, StaticCaptureInitParams>) {
            binding.init_mode_str = "StaticCapture";
            binding.init_data_key =
                    (init.data_key == "(None)") ? "" : init.data_key;
            binding.init_frame = init.frame;
        } else if constexpr (std::is_same_v<T, FirstOutputInitParams>) {
            binding.init_mode_str = "FirstOutput";
        }
    });

    return binding;
}

RecurrentBindingSlotParams RecurrentBindingWidget::paramsFromBinding(
        RecurrentBindingData const & binding) {
    RecurrentBindingSlotParams p;
    p.output_slot_name = binding.output_slot_name;

    if (binding.init_mode_str == "StaticCapture") {
        p.init = StaticCaptureInitParams{
                .data_key = binding.init_data_key,
                .frame = binding.init_frame};
    } else if (binding.init_mode_str == "FirstOutput") {
        p.init = FirstOutputInitParams{};
    } else {
        p.init = ZerosInitParams{};
    }
    return p;
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void RecurrentBindingWidget::_refreshOutputSlotCombo() {
    _auto_param->updateAllowedValues("output_slot_name", _output_slot_names);
}

void RecurrentBindingWidget::_refreshDataKeyCombo() {
    if (!_dm) return;
    auto const type_hint =
            SlotAssembler::dataTypeForEncoder(_recommended_encoder);
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    auto const keys = getKeysForTypes(*_dm, types);
    _auto_param->updateAllowedValues("data_key", keys);
}

}// namespace dl::widget
