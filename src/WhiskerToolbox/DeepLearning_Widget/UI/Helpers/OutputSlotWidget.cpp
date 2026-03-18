/// @file OutputSlotWidget.cpp
/// @brief Implementation of OutputSlotWidget.

#include "OutputSlotWidget.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "DeepLearning/channel_decoding/DecoderParamSchemas.hpp"

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
// Decoder tag ↔ SlotAssembler decoder ID mapping
// ════════════════════════════════════════════════════════════════════════════

namespace {

/// Map from reflect-cpp variant tag (struct name) to SlotAssembler decoder ID.
constexpr struct {
    char const * tag;
    char const * decoder_id;
} kTagToDecoderId[] = {
        {"MaskDecoderParams", "TensorToMask2D"},
        {"PointDecoderParams", "TensorToPoint2D"},
        {"LineDecoderParams", "TensorToLine2D"},
        {"FeatureVectorDecoderParams", "TensorToFeatureVector"},
};

/// Map from SlotAssembler decoder ID to reflect-cpp variant tag.
constexpr struct {
    char const * decoder_id;
    char const * tag;
} kDecoderIdToTag[] = {
        {"TensorToMask2D", "MaskDecoderParams"},
        {"TensorToPoint2D", "PointDecoderParams"},
        {"TensorToLine2D", "LineDecoderParams"},
        {"TensorToFeatureVector", "FeatureVectorDecoderParams"},
};

}// namespace

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

OutputSlotWidget::OutputSlotWidget(
        dl::TensorSlotDescriptor const & slot,
        std::shared_ptr<DataManager> dm,
        QWidget * parent)
    : QWidget(parent),
      _slot_name(slot.name),
      _dm(std::move(dm)),
      _recommended_decoder(slot.recommended_decoder) {

    assert(_dm && "OutputSlotWidget: DataManager must not be null");

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto * group = new QGroupBox(QString::fromStdString(slot.name), this);
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

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
            WhiskerToolbox::Transforms::V2::extractParameterSchema<OutputSlotParams>();
    _auto_param->setSchema(schema);

    if (!_recommended_decoder.empty()) {
        OutputSlotParams initial;
        for (auto const & [decoder_id, tag]: kDecoderIdToTag) {
            if (_recommended_decoder == decoder_id) {
                if (tag == std::string("MaskDecoderParams")) {
                    initial.decoder = dl::MaskDecoderParams{};
                } else if (tag == std::string("PointDecoderParams")) {
                    initial.decoder = dl::PointDecoderParams{};
                } else if (tag == std::string("LineDecoderParams")) {
                    initial.decoder = dl::LineDecoderParams{};
                } else if (tag == std::string("FeatureVectorDecoderParams")) {
                    initial.decoder = dl::FeatureVectorDecoderParams{};
                }
                break;
            }
        }
        auto json = rfl::json::write(initial);
        _auto_param->fromJson(json);
    }

    _refreshTargetCombo();

    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        auto current = params();
        auto const decoder_id =
                _decoderIdFromTag(rfl::json::write(current.decoder));
        if (!decoder_id.empty()) {
            auto const type_hint =
                    SlotAssembler::dataTypeForDecoder(decoder_id);
            auto const types =
                    DataSourceComboHelper::typesFromHint(type_hint);
            auto const keys = getKeysForTypes(*_dm, types);
            _auto_param->updateAllowedValues("data_key", keys);
        }
        emit bindingChanged();
    });
}

OutputSlotWidget::~OutputSlotWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

OutputSlotParams OutputSlotWidget::params() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<OutputSlotParams>(json_str);
    if (result) {
        return result.value();
    }
    return {};
}

void OutputSlotWidget::setParams(OutputSlotParams const & p) {
    auto json = rfl::json::write(p);
    _auto_param->fromJson(json);
}

void OutputSlotWidget::refreshDataSources() {
    _refreshTargetCombo();
}

void OutputSlotWidget::updateDecoderAlternatives(
        std::vector<std::string> const & allowed_tags) {
    _auto_param->updateVariantAlternatives("decoder", allowed_tags);
}

std::string const & OutputSlotWidget::slotName() const {
    return _slot_name;
}

OutputBindingData OutputSlotWidget::toOutputBindingData() const {
    auto const p = params();
    OutputBindingData binding;
    binding.slot_name = _slot_name;
    binding.data_key = (p.data_key == "(None)") ? "" : p.data_key;

    p.decoder.visit([&](auto const & dec) {
        using T = std::decay_t<decltype(dec)>;
        if constexpr (std::is_same_v<T, dl::MaskDecoderParams>) {
            binding.decoder_id = "TensorToMask2D";
            binding.threshold = dec.threshold;
        } else if constexpr (std::is_same_v<T, dl::PointDecoderParams>) {
            binding.decoder_id = "TensorToPoint2D";
            binding.threshold = dec.threshold;
            binding.subpixel = dec.subpixel;
        } else if constexpr (std::is_same_v<T, dl::LineDecoderParams>) {
            binding.decoder_id = "TensorToLine2D";
            binding.threshold = dec.threshold;
        } else if constexpr (std::is_same_v<T, dl::FeatureVectorDecoderParams>) {
            binding.decoder_id = "TensorToFeatureVector";
        }
    });

    return binding;
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void OutputSlotWidget::_refreshTargetCombo() {
    if (!_dm) return;

    auto current = params();
    auto const decoder_id =
            _decoderIdFromTag(rfl::json::write(current.decoder));
    std::string type_hint =
            decoder_id.empty()
                    ? SlotAssembler::dataTypeForDecoder(_recommended_decoder)
                    : SlotAssembler::dataTypeForDecoder(decoder_id);
    if (type_hint.empty()) {
        type_hint = "MaskData";
    }
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    auto const keys = getKeysForTypes(*_dm, types);
    _auto_param->updateAllowedValues("data_key", keys);
}

std::string OutputSlotWidget::_decoderIdFromTag(std::string const & json) {
    for (auto const & [tag, decoder_id]: kTagToDecoderId) {
        if (json.find(tag) != std::string::npos) {
            return decoder_id;
        }
    }
    return {};
}

OutputSlotParams OutputSlotWidget::paramsFromBinding(
        OutputBindingData const & binding) {
    OutputSlotParams p;
    p.data_key = binding.data_key;
    if (binding.decoder_id == "TensorToMask2D") {
        p.decoder = dl::MaskDecoderParams{.threshold = binding.threshold};
    } else if (binding.decoder_id == "TensorToPoint2D") {
        p.decoder = dl::PointDecoderParams{
                .subpixel = binding.subpixel,
                .threshold = binding.threshold};
    } else if (binding.decoder_id == "TensorToLine2D") {
        p.decoder = dl::LineDecoderParams{.threshold = binding.threshold};
    } else if (binding.decoder_id == "TensorToFeatureVector") {
        p.decoder = dl::FeatureVectorDecoderParams{};
    } else {
        p.decoder = dl::MaskDecoderParams{.threshold = binding.threshold};
    }
    return p;
}

}// namespace dl::widget
