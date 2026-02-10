#include "DeepLearningPropertiesWidget.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <filesystem>
#include <iostream>

// ────────────────────────────────────────────────────────────────────────────
// Construction
// ────────────────────────────────────────────────────────────────────────────

DeepLearningPropertiesWidget::DeepLearningPropertiesWidget(
    std::shared_ptr<DeepLearningState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _assembler(std::make_unique<SlotAssembler>()) {

    _buildUi();
    _populateModelCombo();

    connect(_state.get(), &DeepLearningState::modelChanged, this, [this] {
        auto const & id = _state->selectedModelId();
        int const idx = _model_combo->findData(QString::fromStdString(id));
        if (idx >= 0 && idx != _model_combo->currentIndex()) {
            _model_combo->setCurrentIndex(idx);
        }
    });
}

DeepLearningPropertiesWidget::~DeepLearningPropertiesWidget() = default;

// ────────────────────────────────────────────────────────────────────────────
// UI Construction
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_buildUi() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(6, 6, 6, 6);
    main_layout->setSpacing(6);

    // ── Model Selection ──
    {
        auto * group = new QGroupBox(tr("Model"), this);
        auto * form = new QFormLayout(group);

        _model_combo = new QComboBox(group);
        _model_desc_label = new QLabel(group);
        _model_desc_label->setWordWrap(true);
        _model_desc_label->setStyleSheet(
            QStringLiteral("color: gray; font-size: 11px;"));

        form->addRow(tr("Model:"), _model_combo);
        form->addRow(_model_desc_label);
        main_layout->addWidget(group);

        connect(_model_combo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DeepLearningPropertiesWidget::_onModelComboChanged);
    }

    // ── Weights ──
    {
        auto * group = new QGroupBox(tr("Weights"), this);
        auto * vlayout = new QVBoxLayout(group);

        auto * path_row = new QHBoxLayout();
        _weights_path_edit = new QLineEdit(group);
        _weights_path_edit->setPlaceholderText(
            tr("Path to .pte weights file..."));
        _weights_browse_btn = new QPushButton(tr("Browse..."), group);
        path_row->addWidget(_weights_path_edit);
        path_row->addWidget(_weights_browse_btn);
        vlayout->addLayout(path_row);

        _weights_status_label = new QLabel(group);
        _weights_status_label->setStyleSheet(QStringLiteral("color: gray;"));
        vlayout->addWidget(_weights_status_label);
        main_layout->addWidget(group);

        connect(_weights_browse_btn, &QPushButton::clicked,
                this, &DeepLearningPropertiesWidget::_onWeightsBrowseClicked);
        connect(_weights_path_edit, &QLineEdit::editingFinished,
                this, &DeepLearningPropertiesWidget::_onWeightsPathEdited);
    }

    // ── Scroll area for dynamic slot panels ──
    {
        auto * scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        _dynamic_container = new QWidget(scroll);
        _dynamic_layout = new QVBoxLayout(_dynamic_container);
        _dynamic_layout->setContentsMargins(0, 0, 0, 0);
        _dynamic_layout->addStretch();

        scroll->setWidget(_dynamic_container);
        main_layout->addWidget(scroll, /*stretch=*/1);
    }

    // ── Bottom bar ──
    {
        auto * bar = new QHBoxLayout();

        bar->addWidget(new QLabel(tr("Frame:"), this));
        _frame_spin = new QSpinBox(this);
        _frame_spin->setRange(0, 999999);
        _frame_spin->setValue(_state->currentFrame());
        bar->addWidget(_frame_spin);

        connect(_frame_spin, QOverload<int>::of(&QSpinBox::valueChanged),
                _state.get(), &DeepLearningState::setCurrentFrame);

        bar->addWidget(new QLabel(tr("Batch:"), this));
        _batch_size_spin = new QSpinBox(this);
        _batch_size_spin->setRange(1, 9999);
        _batch_size_spin->setValue(_state->batchSize());
        bar->addWidget(_batch_size_spin);

        connect(_batch_size_spin, QOverload<int>::of(&QSpinBox::valueChanged),
                _state.get(), &DeepLearningState::setBatchSize);

        bar->addStretch();

        _run_single_btn = new QPushButton(tr("\u25B6 Run Frame"), this);
        _run_batch_btn = new QPushButton(tr("\u25B6\u25B6 Run Batch"), this);
        _run_single_btn->setEnabled(false);
        _run_batch_btn->setEnabled(false);
        bar->addWidget(_run_single_btn);
        bar->addWidget(_run_batch_btn);

        connect(_run_single_btn, &QPushButton::clicked,
                this, &DeepLearningPropertiesWidget::_onRunSingleFrame);
        connect(_run_batch_btn, &QPushButton::clicked,
                this, &DeepLearningPropertiesWidget::_onRunBatch);

        main_layout->addLayout(bar);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Model Combo
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_populateModelCombo() {
    _model_combo->blockSignals(true);
    _model_combo->clear();
    _model_combo->addItem(tr("(None)"), QString{});

    for (auto const & id : SlotAssembler::availableModelIds()) {
        auto info = SlotAssembler::getModelDisplayInfo(id);
        QString display = info
            ? QString::fromStdString(info->display_name)
            : QString::fromStdString(id);
        _model_combo->addItem(display, QString::fromStdString(id));
    }

    auto const & saved_id = _state->selectedModelId();
    if (!saved_id.empty()) {
        int const idx =
            _model_combo->findData(QString::fromStdString(saved_id));
        if (idx >= 0) {
            _model_combo->setCurrentIndex(idx);
        }
    }
    _model_combo->blockSignals(false);

    _onModelComboChanged(_model_combo->currentIndex());
}

void DeepLearningPropertiesWidget::_onModelComboChanged(int index) {
    auto const model_id =
        _model_combo->itemData(index).toString().toStdString();
    _state->setSelectedModelId(model_id);

    if (model_id.empty()) {
        _model_desc_label->setText(tr("Select a model to configure."));
        _current_info.reset();
        _assembler->resetModel();
    } else {
        _current_info = SlotAssembler::getModelDisplayInfo(model_id);
        if (_current_info) {
            _model_desc_label->setText(
                QString::fromStdString(_current_info->description));
            int const pref = _current_info->preferred_batch_size;
            _batch_size_spin->setValue(pref > 0 ? pref : 1);
            if (_current_info->max_batch_size > 0) {
                _batch_size_spin->setMaximum(_current_info->max_batch_size);
            }
        }
        _assembler->loadModel(model_id);
    }

    _rebuildSlotPanels();
    _updateWeightsStatus();
}

// ────────────────────────────────────────────────────────────────────────────
// Weights
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_onWeightsBrowseClicked() {
    auto const path = QFileDialog::getOpenFileName(
        this,
        tr("Select Model Weights"),
        QString::fromStdString(_state->weightsPath()),
        tr("ExecuTorch Models (*.pte);;All Files (*)"));
    if (!path.isEmpty()) {
        _weights_path_edit->setText(path);
        _state->setWeightsPath(path.toStdString());
        _loadModelIfReady();
    }
}

void DeepLearningPropertiesWidget::_onWeightsPathEdited() {
    _state->setWeightsPath(_weights_path_edit->text().toStdString());
    _loadModelIfReady();
}

void DeepLearningPropertiesWidget::_loadModelIfReady() {
    _updateWeightsStatus();

    auto const & path = _state->weightsPath();
    if (!path.empty() && !_assembler->currentModelId().empty()) {
        try {
            if (std::filesystem::exists(path)) {
                _assembler->loadWeights(path);
                _updateWeightsStatus();
            }
        } catch (std::exception const & e) {
            _weights_status_label->setText(
                tr("Error: %1").arg(QString::fromUtf8(e.what())));
            _weights_status_label->setStyleSheet(
                QStringLiteral("color: red;"));
        }
    }

    bool const ready = _assembler->isModelReady();
    _run_single_btn->setEnabled(ready);
    _run_batch_btn->setEnabled(ready);
}

void DeepLearningPropertiesWidget::_updateWeightsStatus() {
    if (_assembler->currentModelId().empty()) {
        _weights_status_label->setText(tr("No model selected"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: gray;"));
        return;
    }
    auto const & path = _state->weightsPath();
    if (path.empty()) {
        _weights_status_label->setText(tr("No weights file specified"));
        _weights_status_label->setStyleSheet(
            QStringLiteral("color: orange;"));
    } else if (!std::filesystem::exists(path)) {
        _weights_status_label->setText(tr("\u2717 File not found"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: red;"));
    } else if (_assembler->isModelReady()) {
        _weights_status_label->setText(tr("\u2713 Loaded"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        _weights_status_label->setText(tr("File exists, not yet loaded"));
        _weights_status_label->setStyleSheet(
            QStringLiteral("color: orange;"));
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Dynamic Slot Panel Rebuild
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_clearDynamicContent() {
    if (!_dynamic_layout) return;
    QLayoutItem * child = nullptr;
    while ((child = _dynamic_layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
}

void DeepLearningPropertiesWidget::_rebuildSlotPanels() {
    _clearDynamicContent();

    if (!_current_info) {
        _dynamic_layout->addStretch();
        return;
    }

    auto const & inputs = _current_info->inputs;
    auto const & outputs = _current_info->outputs;

    // ── Dynamic (per-frame) inputs ──
    bool has_dynamic = false;
    for (auto const & slot : inputs) {
        if (!slot.is_static && !slot.is_boolean_mask) {
            if (!has_dynamic) {
                _dynamic_layout->addWidget(
                    new QLabel(tr("<b>Dynamic Inputs</b>"),
                               _dynamic_container));
                has_dynamic = true;
            }
            _dynamic_layout->addWidget(_buildDynamicInputGroup(slot));
        }
    }

    // ── Static (memory) inputs ──
    bool has_static = false;
    for (auto const & slot : inputs) {
        if (slot.is_static && !slot.is_boolean_mask) {
            if (!has_static) {
                _dynamic_layout->addWidget(
                    new QLabel(tr("<b>Static Inputs (Memory)</b>"),
                               _dynamic_container));
                has_static = true;
            }
            _dynamic_layout->addWidget(_buildStaticInputGroup(slot));
        }
    }

    // ── Boolean mask inputs ──
    for (auto const & slot : inputs) {
        if (slot.is_boolean_mask) {
            _dynamic_layout->addWidget(_buildBooleanMaskGroup(slot));
        }
    }

    // ── Outputs ──
    if (!outputs.empty()) {
        _dynamic_layout->addWidget(
            new QLabel(tr("<b>Outputs</b>"), _dynamic_container));
    }
    for (auto const & slot : outputs) {
        _dynamic_layout->addWidget(_buildOutputGroup(slot));
    }

    _dynamic_layout->addStretch();
}

// ────────────────────────────────────────────────────────────────────────────
// Slot Panel Builders
// ────────────────────────────────────────────────────────────────────────────

QGroupBox * DeepLearningPropertiesWidget::_buildDynamicInputGroup(
    dl::TensorSlotDescriptor const & slot) {

    auto * group =
        new QGroupBox(QString::fromStdString(slot.name), _dynamic_container);
    auto * form = new QFormLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    // Shape label
    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    form->addRow(tr("Shape:"), new QLabel(shape_str, group));

    // Source data combo
    auto * source_combo = new QComboBox(group);
    source_combo->setObjectName(
        QString::fromStdString("source_" + slot.name));
    _populateDataSourceCombo(
        source_combo,
        SlotAssembler::dataTypeForEncoder(slot.recommended_encoder));
    form->addRow(tr("Source:"), source_combo);

    // Encoder combo
    auto * encoder_combo = new QComboBox(group);
    encoder_combo->setObjectName(
        QString::fromStdString("encoder_" + slot.name));
    for (auto const & enc : SlotAssembler::availableEncoders()) {
        encoder_combo->addItem(QString::fromStdString(enc));
    }
    if (!slot.recommended_encoder.empty()) {
        int const idx = encoder_combo->findText(
            QString::fromStdString(slot.recommended_encoder));
        if (idx >= 0) encoder_combo->setCurrentIndex(idx);
    }
    form->addRow(tr("Encoder:"), encoder_combo);

    // Mode combo
    auto * mode_combo = new QComboBox(group);
    mode_combo->setObjectName(
        QString::fromStdString("mode_" + slot.name));
    for (auto const & m : _modesForEncoder(slot.recommended_encoder)) {
        mode_combo->addItem(QString::fromStdString(m));
    }
    form->addRow(tr("Mode:"), mode_combo);

    // Sigma
    auto * sigma_spin = new QDoubleSpinBox(group);
    sigma_spin->setObjectName(
        QString::fromStdString("sigma_" + slot.name));
    sigma_spin->setRange(0.1, 50.0);
    sigma_spin->setValue(2.0);
    sigma_spin->setSingleStep(0.5);
    form->addRow(tr("Sigma:"), sigma_spin);

    // Re-filter when encoder changes
    connect(encoder_combo, &QComboBox::currentTextChanged, this,
            [this, source_combo, mode_combo](QString const & enc_text) {
                auto const enc_id = enc_text.toStdString();
                _populateDataSourceCombo(
                    source_combo,
                    SlotAssembler::dataTypeForEncoder(enc_id));
                mode_combo->clear();
                for (auto const & m : _modesForEncoder(enc_id)) {
                    mode_combo->addItem(QString::fromStdString(m));
                }
            });

    return group;
}

QGroupBox * DeepLearningPropertiesWidget::_buildStaticInputGroup(
    dl::TensorSlotDescriptor const & slot) {

    auto * group = new QGroupBox(
        QString::fromStdString(slot.name) + tr(" (static)"),
        _dynamic_container);
    auto * layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    layout->addWidget(new QLabel(tr("Shape: ") + shape_str, group));

    auto * form = new QFormLayout();

    auto * source_combo = new QComboBox(group);
    source_combo->setObjectName(
        QString::fromStdString("static_source_" + slot.name));
    _populateDataSourceCombo(
        source_combo,
        SlotAssembler::dataTypeForEncoder(slot.recommended_encoder));
    form->addRow(tr("Source:"), source_combo);

    auto * offset_spin = new QSpinBox(group);
    offset_spin->setObjectName(
        QString::fromStdString("static_offset_" + slot.name));
    offset_spin->setRange(-99999, 0);
    offset_spin->setValue(0);
    offset_spin->setPrefix(QStringLiteral("t"));
    form->addRow(tr("Time Offset:"), offset_spin);

    layout->addLayout(form);
    return group;
}

QGroupBox * DeepLearningPropertiesWidget::_buildBooleanMaskGroup(
    dl::TensorSlotDescriptor const & slot) {

    auto * group = new QGroupBox(
        QString::fromStdString(slot.name) + tr(" (boolean)"),
        _dynamic_container);
    auto * layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    auto * info = new QLabel(
        tr("Active memory slot flags. Automatically managed."), group);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    layout->addWidget(info);

    return group;
}

QGroupBox * DeepLearningPropertiesWidget::_buildOutputGroup(
    dl::TensorSlotDescriptor const & slot) {

    auto * group = new QGroupBox(
        QString::fromStdString(slot.name), _dynamic_container);
    auto * form = new QFormLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    form->addRow(tr("Shape:"), new QLabel(shape_str, group));

    auto * target_combo = new QComboBox(group);
    target_combo->setObjectName(
        QString::fromStdString("target_" + slot.name));
    _populateDataSourceCombo(
        target_combo,
        SlotAssembler::dataTypeForDecoder(slot.recommended_decoder));
    form->addRow(tr("Target:"), target_combo);

    auto * decoder_combo = new QComboBox(group);
    decoder_combo->setObjectName(
        QString::fromStdString("decoder_" + slot.name));
    for (auto const & dec : SlotAssembler::availableDecoders()) {
        decoder_combo->addItem(QString::fromStdString(dec));
    }
    if (!slot.recommended_decoder.empty()) {
        int const idx = decoder_combo->findText(
            QString::fromStdString(slot.recommended_decoder));
        if (idx >= 0) decoder_combo->setCurrentIndex(idx);
    }
    form->addRow(tr("Decoder:"), decoder_combo);

    auto * threshold_spin = new QDoubleSpinBox(group);
    threshold_spin->setObjectName(
        QString::fromStdString("threshold_" + slot.name));
    threshold_spin->setRange(0.01, 1.0);
    threshold_spin->setValue(0.5);
    threshold_spin->setSingleStep(0.05);
    form->addRow(tr("Threshold:"), threshold_spin);

    return group;
}

// ────────────────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_populateDataSourceCombo(
    QComboBox * combo, std::string const & type_hint) {

    QString const current = combo->currentText();
    combo->clear();
    combo->addItem(tr("(None)"));

    if (!_data_manager) return;

    std::vector<std::string> keys;
    if (type_hint == "MediaData") {
        keys = _data_manager->getKeys<MediaData>();
    } else if (type_hint == "PointData") {
        keys = _data_manager->getKeys<PointData>();
    } else if (type_hint == "MaskData") {
        keys = _data_manager->getKeys<MaskData>();
    } else if (type_hint == "LineData") {
        keys = _data_manager->getKeys<LineData>();
    } else {
        keys = _data_manager->getAllKeys();
    }

    for (auto const & key : keys) {
        combo->addItem(QString::fromStdString(key));
    }

    int const idx = combo->findText(current);
    if (idx >= 0) combo->setCurrentIndex(idx);
}

std::vector<std::string> DeepLearningPropertiesWidget::_modesForEncoder(
    std::string const & encoder_id) const {

    if (encoder_id == "ImageEncoder") return {"Raw"};
    if (encoder_id == "Point2DEncoder") return {"Binary", "Heatmap"};
    if (encoder_id == "Mask2DEncoder") return {"Binary"};
    if (encoder_id == "Line2DEncoder") return {"Binary", "Heatmap"};
    return {"Raw", "Binary", "Heatmap"};
}

// ────────────────────────────────────────────────────────────────────────────
// State Sync
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_syncBindingsFromUi() {
    if (!_current_info) return;

    // ── Input bindings ──
    std::vector<SlotBindingData> input_bindings;
    for (auto const & slot : _current_info->inputs) {
        if (slot.is_static || slot.is_boolean_mask) continue;

        SlotBindingData binding;
        binding.slot_name = slot.name;

        auto * source = _dynamic_container->findChild<QComboBox *>(
            QString::fromStdString("source_" + slot.name));
        auto * encoder = _dynamic_container->findChild<QComboBox *>(
            QString::fromStdString("encoder_" + slot.name));
        auto * mode = _dynamic_container->findChild<QComboBox *>(
            QString::fromStdString("mode_" + slot.name));
        auto * sigma = _dynamic_container->findChild<QDoubleSpinBox *>(
            QString::fromStdString("sigma_" + slot.name));

        if (source) binding.data_key = source->currentText().toStdString();
        if (encoder) binding.encoder_id = encoder->currentText().toStdString();
        if (mode) binding.mode = mode->currentText().toStdString();
        if (sigma) binding.gaussian_sigma = static_cast<float>(sigma->value());

        if (!binding.data_key.empty() && binding.data_key != "(None)") {
            input_bindings.push_back(std::move(binding));
        }
    }
    _state->setInputBindings(std::move(input_bindings));

    // ── Output bindings ──
    std::vector<OutputBindingData> output_bindings;
    for (auto const & slot : _current_info->outputs) {
        OutputBindingData binding;
        binding.slot_name = slot.name;

        auto * target = _dynamic_container->findChild<QComboBox *>(
            QString::fromStdString("target_" + slot.name));
        auto * decoder = _dynamic_container->findChild<QComboBox *>(
            QString::fromStdString("decoder_" + slot.name));
        auto * threshold = _dynamic_container->findChild<QDoubleSpinBox *>(
            QString::fromStdString("threshold_" + slot.name));

        if (target) binding.data_key = target->currentText().toStdString();
        if (decoder) binding.decoder_id = decoder->currentText().toStdString();
        if (threshold)
            binding.threshold = static_cast<float>(threshold->value());

        if (!binding.data_key.empty() && binding.data_key != "(None)") {
            output_bindings.push_back(std::move(binding));
        }
    }
    _state->setOutputBindings(std::move(output_bindings));

    // ── Static inputs ──
    std::vector<StaticInputData> static_inputs;
    for (auto const & slot : _current_info->inputs) {
        if (!slot.is_static || slot.is_boolean_mask) continue;

        StaticInputData si;
        si.slot_name = slot.name;
        si.memory_index = 0;

        auto * source = _dynamic_container->findChild<QComboBox *>(
            QString::fromStdString("static_source_" + slot.name));
        auto * offset = _dynamic_container->findChild<QSpinBox *>(
            QString::fromStdString("static_offset_" + slot.name));

        if (source) si.data_key = source->currentText().toStdString();
        if (offset) si.time_offset = offset->value();

        if (!si.data_key.empty() && si.data_key != "(None)") {
            static_inputs.push_back(std::move(si));
        }
    }
    _state->setStaticInputs(std::move(static_inputs));
}

// ────────────────────────────────────────────────────────────────────────────
// Run Actions
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_onRunSingleFrame() {
    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    _syncBindingsFromUi();

    try {
        int const frame = _state->currentFrame();

        // Determine source image size from primary media binding
        ImageSize source_size{256, 256};
        for (auto const & binding : _state->inputBindings()) {
            auto media = _data_manager->getData<MediaData>(binding.data_key);
            if (media) {
                source_size = media->getImageSize();
                break;
            }
        }

        _assembler->runSingleFrame(
            *_data_manager,
            _state->inputBindings(),
            _state->staticInputs(),
            _state->outputBindings(),
            frame,
            source_size);

        _weights_status_label->setText(
            tr("\u2713 Inference complete (frame %1)").arg(frame));
        _weights_status_label->setStyleSheet(QStringLiteral("color: green;"));

    } catch (std::exception const & e) {
        QMessageBox::critical(
            this, tr("Inference Error"),
            tr("Forward pass failed:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void DeepLearningPropertiesWidget::_onRunBatch() {
    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    // TODO: Implement batch inference with progress reporting.
    QMessageBox::information(
        this, tr("Batch Inference"),
        tr("Batch inference is not yet implemented.\n"
           "Use \"Run Frame\" for single-frame inference."));
}
