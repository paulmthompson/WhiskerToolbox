/// @file PostEncoderWidget.cpp
/// @brief Implementation of PostEncoderWidget.

#include "PostEncoderWidget.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/post_encoder/PostEncoderParamSchemas.hpp"
#include "DeepLearning/post_encoder/SpatialPointExtractModule.hpp"
#include "DeepLearning_Widget/Core/DeepLearningParamSchemasUIHints.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"
#include "Media/Media_Data.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <QGroupBox>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>

namespace dl::widget {

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

PostEncoderWidget::PostEncoderWidget(
        std::shared_ptr<DeepLearningState> state,
        std::shared_ptr<DataManager> dm,
        SlotAssembler * assembler,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _dm(std::move(dm)),
      _assembler(assembler) {

    assert(_state && "PostEncoderWidget: state must not be null");
    assert(_dm && "PostEncoderWidget: DataManager must not be null");
    assert(_assembler && "PostEncoderWidget: SlotAssembler must not be null");

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto * group = new QGroupBox(tr("Post-Encoder Module"), this);
    group->setObjectName(QStringLiteral("post_encoder_group"));
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);

    auto schema =
            extractParameterSchema<
                    PostEncoderSlotParams>();
    _auto_param->setSchema(schema);

    setParams(_state->postEncoderParams());

    refreshDataSources();

    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        _applyToStateAndAssembler();
        emit bindingChanged();
    });
}

PostEncoderWidget::~PostEncoderWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

PostEncoderSlotParams PostEncoderWidget::params() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<PostEncoderSlotParams>(json_str);
    if (result) {
        return result.value();
    }
    return {};
}

void PostEncoderWidget::setParams(PostEncoderSlotParams const & p) {
    auto json = rfl::json::write(p);
    _auto_param->fromJson(json);
    _applyToStateAndAssembler();
}

void PostEncoderWidget::refreshDataSources() {
    if (!_dm) return;
    auto const types = DataSourceComboHelper::typesFromHint("PointData");
    auto const keys = getKeysForTypes(*_dm, types);
    _auto_param->updateAllowedValues("point_key", keys);
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void PostEncoderWidget::_applyToStateAndAssembler() {
    auto const p = params();
    _state->setPostEncoderParams(p);

    if (_assembler->isModelReady()) {
        _assembler->configurePostEncoderModule(p.module, _sourceImageSize());
    }
}

ImageSize PostEncoderWidget::_sourceImageSize() const {
    ImageSize source_size{256, 256};
    for (auto const & binding: _state->inputBindings()) {
        auto media = _dm->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }
    return source_size;
}

}// namespace dl::widget
