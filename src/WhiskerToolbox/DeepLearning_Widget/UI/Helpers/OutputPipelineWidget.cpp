/**
 * @file OutputPipelineWidget.cpp
 * @brief Implementation of OutputPipelineWidget.
 */

#include "OutputPipelineWidget.hpp"

#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/pipeline/OutputPipeline.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>

namespace dl::widget {

namespace {

[[nodiscard]] bool isSpatialDecoder(std::string const & step_id) {
    return step_id == "TensorToMask2D" || step_id == "TensorToPoint2D" ||
           step_id == "TensorToLine2D";
}

[[nodiscard]] int findComboData(QComboBox const * combo, std::string const & value) {
    return combo->findData(QString::fromStdString(value));
}

}// namespace

OutputPipelineWidget::OutputPipelineWidget(std::string model_id,
                                           dl::TensorSlotDescriptor slot,
                                           std::shared_ptr<DataManager> dm,
                                           QWidget * parent)
    : QWidget(parent),
      _model_id(std::move(model_id)),
      _slot(std::move(slot)),
      _dm(std::move(dm)) {
    assert(_dm && "OutputPipelineWidget: DataManager must not be null");
    _buildUi();
    _populateTransformCombo();

    auto pipeline = _slot.recommended_pipeline;
    if (pipeline.empty()) {
        pipeline = defaultOutputPipeline(
                _model_id, _slot.name, _slot.shape);
    }
    OutputBindingData initial;
    initial.slot_name = _slot.name;
    initial.pipeline = std::move(pipeline);
    setBinding(initial);
    refreshDataSources();
}

OutputPipelineWidget::~OutputPipelineWidget() = default;

std::string const & OutputPipelineWidget::slotName() const {
    return _slot.name;
}

OutputBindingData OutputPipelineWidget::toOutputBindingData() const {
    OutputBindingData binding;
    binding.slot_name = _slot.name;
    binding.data_key = _target_combo->currentText().toStdString();
    if (binding.data_key == "(None)") {
        binding.data_key.clear();
    }
    binding.pipeline = _currentPipeline();

    return binding;
}

void OutputPipelineWidget::setBinding(OutputBindingData const & binding) {
    auto pipeline = binding.pipeline;
    if (pipeline.empty()) {
        pipeline = defaultOutputPipeline(
                _model_id, _slot.name, _slot.shape);
    }

    std::string transform_id;
    std::string decoder_id;
    for (auto const & step: pipeline) {
        if (isTerminalPipelineStep(step.step_id)) {
            decoder_id = step.step_id;
            if (step.step_id == "TensorToMask2D") {
                auto const params = maskDecoderParamsForStep(step);
                _threshold_spin->setValue(static_cast<double>(params.threshold));
            } else if (step.step_id == "TensorToPoint2D") {
                auto const params = pointDecoderParamsForStep(step);
                _threshold_spin->setValue(static_cast<double>(params.threshold));
                _subpixel_check->setChecked(params.subpixel);
            } else if (step.step_id == "TensorToLine2D") {
                auto const params = lineDecoderParamsForStep(step);
                _threshold_spin->setValue(static_cast<double>(params.threshold));
            }
        } else if (isTensorTransformPipelineStep(step.step_id)) {
            transform_id = step.step_id;
            auto const params = spatialPointParamsForStep(step);
            if (!params.point_key.empty()) {
                int const idx = _spatial_point_combo->findText(
                        QString::fromStdString(params.point_key));
                if (idx >= 0) {
                    _spatial_point_combo->setCurrentIndex(idx);
                }
            }
            auto const interpolation =
                    params.interpolation == InterpolationMode::Bilinear
                            ? QStringLiteral("bilinear")
                            : QStringLiteral("nearest");
            int const idx = _interpolation_combo->findData(interpolation);
            if (idx >= 0) {
                _interpolation_combo->setCurrentIndex(idx);
            }
        }
    }

    int const transform_idx = transform_id.empty()
                                      ? _transform_combo->findData(QString{})
                                      : findComboData(_transform_combo, transform_id);
    if (transform_idx >= 0) {
        _transform_combo->setCurrentIndex(transform_idx);
    }

    _onPipelineShapeChanged();
    int const decoder_idx = findComboData(_decoder_combo, decoder_id);
    if (decoder_idx >= 0) {
        _decoder_combo->setCurrentIndex(decoder_idx);
    }

    if (!binding.data_key.empty()) {
        int const target_idx =
                _target_combo->findText(QString::fromStdString(binding.data_key));
        if (target_idx >= 0) {
            _target_combo->setCurrentIndex(target_idx);
        }
    }
    _syncParameterVisibility();
    _updatePipelineSummary();
}

void OutputPipelineWidget::refreshDataSources() {
    _refreshPointCombo();
    _refreshTargetCombo();
}

void OutputPipelineWidget::_buildUi() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    _group = new QGroupBox(QString::fromStdString(_slot.name), this);
    main_layout->addWidget(_group);

    auto * form = new QFormLayout(_group);
    if (!_slot.description.empty()) {
        _group->setToolTip(QString::fromStdString(_slot.description));
    }

    _input_shape_label = new QLabel(_shapeText(_slot.shape), _group);
    form->addRow(tr("Model Output:"), _input_shape_label);

    _transform_combo = new QComboBox(_group);
    form->addRow(tr("Tensor Step:"), _transform_combo);

    _spatial_point_combo = new QComboBox(_group);
    form->addRow(tr("Point Source:"), _spatial_point_combo);

    _interpolation_combo = new QComboBox(_group);
    _interpolation_combo->addItem(tr("Nearest"), QStringLiteral("nearest"));
    _interpolation_combo->addItem(tr("Bilinear"), QStringLiteral("bilinear"));
    form->addRow(tr("Interpolation:"), _interpolation_combo);

    _decoder_combo = new QComboBox(_group);
    form->addRow(tr("Decoder:"), _decoder_combo);

    _threshold_spin = new QDoubleSpinBox(_group);
    _threshold_spin->setRange(0.01, 1.0);
    _threshold_spin->setSingleStep(0.05);
    _threshold_spin->setValue(0.5);
    form->addRow(tr("Threshold:"), _threshold_spin);

    _subpixel_check = new QCheckBox(tr("Subpixel refinement"), _group);
    _subpixel_check->setChecked(true);
    form->addRow(QString{}, _subpixel_check);

    _target_combo = new QComboBox(_group);
    form->addRow(tr("Target:"), _target_combo);

    _pipeline_summary_label = new QLabel(_group);
    _pipeline_summary_label->setWordWrap(true);
    _pipeline_summary_label->setStyleSheet(
            QStringLiteral("color: gray; font-size: 11px;"));
    form->addRow(tr("Pipeline:"), _pipeline_summary_label);

    auto const emit_changed = [this]() {
        _syncParameterVisibility();
        _refreshTargetCombo();
        _updatePipelineSummary();
        emit bindingChanged();
    };

    connect(_transform_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this, emit_changed]() {
                _onPipelineShapeChanged();
                emit_changed();
            });
    connect(_decoder_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            emit_changed);
    connect(_spatial_point_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            emit_changed);
    connect(_interpolation_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            emit_changed);
    connect(_threshold_spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            emit_changed);
    connect(_subpixel_check, &QCheckBox::toggled, this, emit_changed);
    connect(_target_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            emit_changed);
}

void OutputPipelineWidget::_populateTransformCombo() {
    _transform_combo->clear();
    _transform_combo->addItem(tr("None"), QString{});
    for (auto const & step_id: validNextPipelineStepIds(_slot.shape)) {
        if (isTensorTransformPipelineStep(step_id)) {
            _transform_combo->addItem(_displayName(step_id),
                                      QString::fromStdString(step_id));
        }
    }
    _transform_combo->setVisible(_transform_combo->count() > 1);
}

void OutputPipelineWidget::_populateDecoderCombo(
        std::vector<int64_t> const & tensor_shape) {
    auto const current = _selectedDecoderId();
    _decoder_combo->blockSignals(true);
    _decoder_combo->clear();
    for (auto const & step_id: validNextPipelineStepIds(tensor_shape)) {
        if (isTerminalPipelineStep(step_id)) {
            _decoder_combo->addItem(_displayName(step_id),
                                    QString::fromStdString(step_id));
        }
    }
    int const idx = findComboData(_decoder_combo, current);
    if (idx >= 0) {
        _decoder_combo->setCurrentIndex(idx);
    }
    _decoder_combo->blockSignals(false);
}

void OutputPipelineWidget::_refreshPointCombo() {
    auto const current = _spatial_point_combo->currentText();
    _spatial_point_combo->blockSignals(true);
    _spatial_point_combo->clear();
    _spatial_point_combo->addItem(tr("(None)"));
    auto const keys =
            getKeysForTypes(*_dm, DataSourceComboHelper::typesFromHint("PointData"));
    for (auto const & key: keys) {
        _spatial_point_combo->addItem(QString::fromStdString(key));
    }
    int const idx = _spatial_point_combo->findText(current);
    if (idx >= 0) {
        _spatial_point_combo->setCurrentIndex(idx);
    }
    _spatial_point_combo->blockSignals(false);
}

void OutputPipelineWidget::_refreshTargetCombo() {
    auto const current = _target_combo->currentText();
    auto const decoder_id = _selectedDecoderId();
    auto const type_hint = outputDataTypeForStep(decoder_id);
    _target_combo->blockSignals(true);
    _target_combo->clear();
    _target_combo->addItem(tr("(None)"));
    auto const keys =
            getKeysForTypes(*_dm, DataSourceComboHelper::typesFromHint(type_hint));
    for (auto const & key: keys) {
        _target_combo->addItem(QString::fromStdString(key));
    }
    int const idx = _target_combo->findText(current);
    if (idx >= 0) {
        _target_combo->setCurrentIndex(idx);
    }
    _target_combo->blockSignals(false);
}

void OutputPipelineWidget::_syncParameterVisibility() {
    auto const transform_id = _selectedTransformId();
    bool const spatial_point = transform_id == "spatial_point";
    _spatial_point_combo->setVisible(spatial_point);
    _interpolation_combo->setVisible(spatial_point);

    auto const decoder_id = _selectedDecoderId();
    bool const spatial_decoder = isSpatialDecoder(decoder_id);
    _threshold_spin->setVisible(spatial_decoder);
    _subpixel_check->setVisible(decoder_id == "TensorToPoint2D");
}

void OutputPipelineWidget::_updatePipelineSummary() {
    QStringList labels;
    labels << QString::fromStdString(_slot.name);
    for (auto const & step: _currentPipeline()) {
        labels << _displayName(step.step_id);
    }
    auto const validation = validateOutputPipeline(_slot.shape, _currentPipeline());
    auto summary = labels.join(QStringLiteral(" -> "));
    if (!validation.output_data_type.empty()) {
        summary += tr(" -> %1").arg(QString::fromStdString(validation.output_data_type));
    }
    if (!validation.valid) {
        summary += tr(" (%1)").arg(QString::fromStdString(validation.message));
    }
    _pipeline_summary_label->setText(summary);
}

void OutputPipelineWidget::_onPipelineShapeChanged() {
    _populateDecoderCombo(_shapeAfterTransform());
    _syncParameterVisibility();
    _refreshTargetCombo();
    _updatePipelineSummary();
}

std::vector<dl::OutputPipelineStepSpec> OutputPipelineWidget::_currentPipeline() const {
    std::vector<dl::OutputPipelineStepSpec> pipeline;
    auto const transform_id = _selectedTransformId();
    if (!transform_id.empty()) {
        OutputPipelineStepSpec transform;
        transform.step_id = transform_id;
        if (transform_id == "spatial_point") {
            SpatialPointModuleParams params;
            auto const point_key = _spatial_point_combo->currentText().toStdString();
            if (!point_key.empty() && point_key != "(None)") {
                params.point_key = point_key;
            }
            params.interpolation =
                    _interpolation_combo->currentData().toString() == "bilinear"
                            ? InterpolationMode::Bilinear
                            : InterpolationMode::Nearest;
            transform.parameters = OutputPipelineStepParameters{params};
        }
        pipeline.push_back(std::move(transform));
    }

    auto const decoder_id = _selectedDecoderId();
    if (!decoder_id.empty()) {
        OutputPipelineStepSpec decoder;
        decoder.step_id = decoder_id;
        if (decoder_id == "TensorToMask2D") {
            decoder.parameters = OutputPipelineStepParameters{
                    MaskDecoderParams{.threshold =
                                              static_cast<float>(_threshold_spin->value())}};
        } else if (decoder_id == "TensorToPoint2D") {
            decoder.parameters = OutputPipelineStepParameters{
                    PointDecoderParams{.subpixel = _subpixel_check->isChecked(),
                                       .threshold = static_cast<float>(
                                               _threshold_spin->value())}};
        } else if (decoder_id == "TensorToLine2D") {
            decoder.parameters = OutputPipelineStepParameters{
                    LineDecoderParams{.threshold =
                                              static_cast<float>(_threshold_spin->value())}};
        } else if (decoder_id == "TensorToFeatureVector") {
            decoder.parameters = OutputPipelineStepParameters{
                    FeatureVectorDecoderParams{}};
        }
        pipeline.push_back(std::move(decoder));
    }
    return pipeline;
}

std::vector<int64_t> OutputPipelineWidget::_shapeAfterTransform() const {
    auto const transform_id = _selectedTransformId();
    if (transform_id.empty()) {
        return _slot.shape;
    }
    auto const shape = propagatePipelineStepShape(_slot.shape, transform_id);
    return shape.value_or(_slot.shape);
}

std::string OutputPipelineWidget::_selectedTransformId() const {
    return _transform_combo->currentData().toString().toStdString();
}

std::string OutputPipelineWidget::_selectedDecoderId() const {
    return _decoder_combo->currentData().toString().toStdString();
}

QString OutputPipelineWidget::_shapeText(std::vector<int64_t> const & shape) {
    QStringList parts;
    for (auto const dim: shape) {
        parts << QString::number(dim);
    }
    return parts.join(QStringLiteral(" x "));
}

QString OutputPipelineWidget::_displayName(std::string const & step_id) {
    auto const desc = pipelineStepDescriptor(step_id);
    return QString::fromStdString(desc.has_value() ? desc->display_name : step_id);
}

}// namespace dl::widget
