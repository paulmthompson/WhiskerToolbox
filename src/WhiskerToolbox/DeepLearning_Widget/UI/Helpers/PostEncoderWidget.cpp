/// @file PostEncoderWidget.cpp
/// @brief Implementation of PostEncoderWidget.

#include "PostEncoderWidget.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/post_encoder/PostEncoderModuleRegistry.hpp"
#include "Media/Media_Data.hpp"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <cassert>

namespace dl::widget {

namespace {

constexpr char const * kNoneModuleKey = "none";

[[nodiscard]] bool isNoneModuleKey(std::string const & key) {
    return key.empty() || key == kNoneModuleKey;
}

}// namespace

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

    auto * module_label = new QLabel(tr("Module:"), group);
    group_layout->addWidget(module_label);

    _module_combo = new QComboBox(group);
    _module_combo->setObjectName(QStringLiteral("post_encoder_module_combo"));
    group_layout->addWidget(_module_combo);

    _description_label = new QLabel(group);
    _description_label->setWordWrap(true);
    _description_label->setStyleSheet(QStringLiteral("color: gray; font-size: 9pt;"));
    _description_label->setVisible(false);
    group_layout->addWidget(_description_label);

    auto * params_container = new QWidget(group);
    _params_layout = new QVBoxLayout(params_container);
    _params_layout->setContentsMargins(0, 0, 0, 0);
    group_layout->addWidget(params_container);

    _populateModuleCombo();

    connect(_module_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (_suppress_signals) {
            return;
        }
        _rebuildParamsPanel("{}");
        refreshDataSources();
        _applyToStateAndAssembler();
        emit bindingChanged();
    });

    setParams(_state->postEncoderParams());
    refreshDataSources();
}

PostEncoderWidget::~PostEncoderWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

dl::PostEncoderStepDescriptor PostEncoderWidget::params() const {
    dl::PostEncoderStepDescriptor p;
    p.module_key = _selectedModuleKey();

    if (_auto_param) {
        p.parameters_json = _auto_param->toJson();
    } else {
        p.parameters_json = "{}";
    }

    return p;
}

void PostEncoderWidget::setParams(dl::PostEncoderStepDescriptor const & p) {
    _suppress_signals = true;

    std::string const key =
            isNoneModuleKey(p.module_key) ? kNoneModuleKey : p.module_key;

    int const index = _module_combo->findData(QString::fromStdString(key));
    if (index >= 0) {
        _module_combo->setCurrentIndex(index);
    } else {
        _module_combo->setCurrentIndex(0);
    }

    _rebuildParamsPanel(p.parameters_json);
    refreshDataSources();

    _suppress_signals = false;
    _applyToStateAndAssembler();
}

void PostEncoderWidget::syncToAssembler() {
    if (!_assembler->currentModelId().empty()) {
        _assembler->configurePostEncoderModule(params(), _sourceImageSize());
    }
}

void PostEncoderWidget::refreshDataSources() {
    if (!_dm || !_auto_param) {
        return;
    }
    if (_selectedModuleKey() != "spatial_point") {
        return;
    }
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

    if (!_assembler->currentModelId().empty()) {
        _assembler->configurePostEncoderModule(p, _sourceImageSize());
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

void PostEncoderWidget::_populateModuleCombo() {
    _module_combo->clear();
    _module_combo->addItem(tr("(None)"), QString::fromStdString(kNoneModuleKey));

    auto const & registry = PostEncoderModuleRegistry::instance();
    for (auto const & key: registry.moduleKeys()) {
        auto const meta = registry.getMetadata(key);
        QString const label = meta
                                      ? QString::fromStdString(meta->display_name)
                                      : QString::fromStdString(key);
        _module_combo->addItem(label, QString::fromStdString(key));
    }
}

std::string PostEncoderWidget::_selectedModuleKey() const {
    return _module_combo->currentData().toString().toStdString();
}

void PostEncoderWidget::_rebuildParamsPanel(std::string const & params_json) {
    _clearParamsPanel();

    auto const key = _selectedModuleKey();
    if (isNoneModuleKey(key)) {
        _description_label->clear();
        _description_label->setVisible(false);
        return;
    }

    auto const & registry = PostEncoderModuleRegistry::instance();
    auto const meta = registry.getMetadata(key);
    if (meta && !meta->description.empty()) {
        _description_label->setText(QString::fromStdString(meta->description));
        _description_label->setVisible(true);
    } else {
        _description_label->clear();
        _description_label->setVisible(false);
    }

    auto const schema = registry.getSchema(key);
    if (!schema || schema->fields.empty()) {
        auto * label = new QLabel(
                tr("This module has no configurable parameters."),
                this);
        label->setStyleSheet(QStringLiteral("color: gray; font-style: italic;"));
        _params_layout->addWidget(label);
        return;
    }

    _auto_param = new AutoParamWidget(this);
    _auto_param->setSchema(*schema);

    // Populate dynamic combos before fromJson so restored values remain valid.
    refreshDataSources();

    if (!params_json.empty() && params_json != "{}") {
        _auto_param->fromJson(params_json);
    }

    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        if (_suppress_signals) {
            return;
        }
        _applyToStateAndAssembler();
        emit bindingChanged();
    });

    _params_layout->addWidget(_auto_param);
}

void PostEncoderWidget::_clearParamsPanel() {
    if (_auto_param) {
        _params_layout->removeWidget(_auto_param);
        _auto_param->deleteLater();
        _auto_param = nullptr;
    }

    while (_params_layout->count() > 0) {
        auto * item = _params_layout->takeAt(0);
        if (auto * widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

}// namespace dl::widget
