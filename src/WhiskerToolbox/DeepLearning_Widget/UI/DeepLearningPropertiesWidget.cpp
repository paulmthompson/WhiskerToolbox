#include "DeepLearningPropertiesWidget.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>

#include "StateManagement/AppFileDialog.hpp"
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

    // Register for DataManager data add/delete notifications
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _refreshDataSourceCombos();
        });
    }

    connect(_state.get(), &DeepLearningState::modelChanged, this, [this] {
        auto const & id = _state->selectedModelId();
        int const idx = _model_combo->findData(QString::fromStdString(id));
        if (idx >= 0 && idx != _model_combo->currentIndex()) {
            _model_combo->setCurrentIndex(idx);
        }
    });
}

DeepLearningPropertiesWidget::~DeepLearningPropertiesWidget() {
    if (_data_manager && _dm_observer_id >= 0) {
        _data_manager->removeObserver(_dm_observer_id);
        _dm_observer_id = -1;
    }
}

SlotAssembler * DeepLearningPropertiesWidget::assembler() const {
    return _assembler.get();
}

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
                tr("Path to model weights file..."));
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
        _run_recurrent_btn = new QPushButton(tr("\u21BB Recurrent"), this);
        _run_recurrent_btn->setToolTip(
                tr("Run sequential recurrent inference over a range of frames.\n"
                   "Each frame's output feeds into the next frame's input."));
        _predict_current_frame_btn = new QPushButton(tr("\u25B6 Current"), this);
        _predict_current_frame_btn->setToolTip(tr("Predict at current time position from timeline"));
        _run_single_btn->setEnabled(false);
        _run_batch_btn->setEnabled(false);
        _run_recurrent_btn->setEnabled(false);
        _predict_current_frame_btn->setEnabled(false);
        bar->addWidget(_run_single_btn);
        bar->addWidget(_predict_current_frame_btn);
        bar->addWidget(_run_batch_btn);
        bar->addWidget(_run_recurrent_btn);

        connect(_run_single_btn, &QPushButton::clicked,
                this, &DeepLearningPropertiesWidget::_onRunSingleFrame);
        connect(_run_batch_btn, &QPushButton::clicked,
                this, &DeepLearningPropertiesWidget::_onRunBatch);
        connect(_run_recurrent_btn, &QPushButton::clicked,
                this, &DeepLearningPropertiesWidget::_onRunRecurrentSequence);
        connect(_predict_current_frame_btn, &QPushButton::clicked,
                this, &DeepLearningPropertiesWidget::_onPredictCurrentFrame);

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

    for (auto const & id: SlotAssembler::availableModelIds()) {
        auto info = SlotAssembler::getModelDisplayInfo(id);
        QString const display = info
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

            // Set initial batch size from model's batch mode
            auto const & mode = _current_info->batch_mode;
            if (auto const * f = std::get_if<dl::FixedBatch>(&mode)) {
                _batch_size_spin->setValue(f->size);
            } else if (std::holds_alternative<dl::RecurrentOnlyBatch>(mode)) {
                _batch_size_spin->setValue(1);
            } else {
                int const pref = _current_info->preferred_batch_size;
                _batch_size_spin->setValue(pref > 0 ? pref : 1);
            }
            int const max_b = dl::maxBatchSizeFromMode(mode);
            if (max_b > 0) {
                _batch_size_spin->setMaximum(max_b);
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
    auto const path = AppFileDialog::getOpenFileName(
            this,
            QStringLiteral("import_model_weights"),
            tr("Select Model Weights"),
#ifdef DL_HAS_EXECUTORCH
            tr("Model Files (*.pt2 *.pt *.pte);;AOT Inductor (*.pt2);;TorchScript (*.pt);;ExecuTorch (*.pte);;All Files (*)"),
#else
            tr("Model Files (*.pt2 *.pt);;AOT Inductor (*.pt2);;TorchScript (*.pt);;All Files (*)"),
#endif
            QString::fromStdString(_state->weightsPath()));
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
    _run_recurrent_btn->setEnabled(ready);
    _predict_current_frame_btn->setEnabled(ready && _current_time_position.has_value());
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
    for (auto const & slot: inputs) {
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
    for (auto const & slot: inputs) {
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
    for (auto const & slot: inputs) {
        if (slot.is_boolean_mask) {
            _dynamic_layout->addWidget(_buildBooleanMaskGroup(slot));
        }
    }

    // ── Recurrent (feedback) inputs ──
    // Show a recurrent binding panel for each non-sequence static input slot
    // that could serve as a recurrent feedback target. Sequence slots handle
    // recurrent bindings per-position via the unified sequence editor.
    bool has_recurrent = false;
    for (auto const & slot: inputs) {
        if (slot.is_static && !slot.is_boolean_mask && !slot.hasSequenceDim()) {
            if (!has_recurrent) {
                _dynamic_layout->addWidget(
                        new QLabel(tr("<b>Recurrent (Feedback) Inputs</b>"),
                                   _dynamic_container));
                has_recurrent = true;
            }
            _dynamic_layout->addWidget(
                    _buildRecurrentInputGroup(slot, outputs));
        }
    }

    // ── Outputs ──
    if (!outputs.empty()) {
        _dynamic_layout->addWidget(
                new QLabel(tr("<b>Outputs</b>"), _dynamic_container));
    }
    for (auto const & slot: outputs) {
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
    for (auto const & enc: SlotAssembler::availableEncoders()) {
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
    for (auto const & m: _modesForEncoder(slot.recommended_encoder)) {
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

    // Time offset
    auto * time_offset_spin = new QSpinBox(group);
    time_offset_spin->setObjectName(
            QString::fromStdString("time_offset_" + slot.name));
    time_offset_spin->setRange(-99999, 99999);
    time_offset_spin->setValue(0);
    time_offset_spin->setPrefix(QStringLiteral("t"));
    time_offset_spin->setToolTip(
            tr("Temporal offset applied to each frame during encoding.\n"
               "E.g. -1 reads one frame behind, +1 reads one frame ahead.\n"
               "Values are clamped to [0, max_frame]."));
    form->addRow(tr("Time Offset:"), time_offset_spin);

    // Re-filter when encoder changes
    connect(encoder_combo, &QComboBox::currentTextChanged, this,
            [this, source_combo, mode_combo](QString const & enc_text) {
                auto const enc_id = enc_text.toStdString();
                _populateDataSourceCombo(
                        source_combo,
                        SlotAssembler::dataTypeForEncoder(enc_id));
                mode_combo->clear();
                for (auto const & m: _modesForEncoder(enc_id)) {
                    mode_combo->addItem(QString::fromStdString(m));
                }
            });

    return group;
}

QGroupBox * DeepLearningPropertiesWidget::_buildStaticInputGroup(
        dl::TensorSlotDescriptor const & slot) {

    auto * group = new QGroupBox(
            QString::fromStdString(slot.name) + tr(" (memory)"),
            _dynamic_container);
    auto * layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    // Info label
    auto * info = new QLabel(
            tr("Relative: re-encodes each run at current_frame + offset.\n"
               "Absolute: capture once at a chosen frame and reuse."),
            group);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    layout->addWidget(info);

    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    layout->addWidget(new QLabel(tr("Shape: ") + shape_str, group));

    if (slot.hasSequenceDim()) {
        // ── Sequence slot: multi-entry manager ──
        int64_t const seq_len = slot.shape[static_cast<std::size_t>(slot.sequence_dim)];
        layout->addWidget(new QLabel(
                tr("Sequence length: %1 (dim %2)")
                        .arg(seq_len)
                        .arg(slot.sequence_dim),
                group));

        // Container for sequence entries
        auto * entries_container = new QWidget(group);
        entries_container->setObjectName(
                QString::fromStdString("seq_entries_" + slot.name));
        auto * entries_layout = new QVBoxLayout(entries_container);
        entries_layout->setContentsMargins(0, 0, 0, 0);
        entries_layout->setSpacing(4);
        layout->addWidget(entries_container);

        // Determine how many entries to show initially from state
        int initial_count = 0;
        for (auto const & si: _state->staticInputs()) {
            if (si.slot_name == slot.name) {
                initial_count = std::max(initial_count, si.memory_index + 1);
            }
        }
        // Show at least 1 entry for user convenience
        if (initial_count == 0) initial_count = 1;

        auto slot_name = slot.name;

        // Add entry rows
        for (int idx = 0; idx < initial_count; ++idx) {
            _addSequenceEntryRow(entries_container, slot, idx);
        }

        // Add / Remove buttons
        auto * btn_row = new QHBoxLayout();
        auto * add_btn = new QPushButton(tr("+ Add Entry"), group);
        add_btn->setToolTip(
                tr("Add another sequence position (up to %1)").arg(seq_len));
        auto * remove_btn = new QPushButton(tr("- Remove Last"), group);
        remove_btn->setToolTip(tr("Remove the last sequence entry"));
        btn_row->addWidget(add_btn);
        btn_row->addWidget(remove_btn);
        btn_row->addStretch();
        layout->addLayout(btn_row);

        // Wire add button
        connect(add_btn, &QPushButton::clicked, this,
                [this, entries_container, slot, seq_len] {
                    int const current_count = static_cast<int>(
                            entries_container->findChildren<QGroupBox *>(
                                                     Qt::FindDirectChildrenOnly)
                                    .size());
                    if (current_count >= static_cast<int>(seq_len)) {
                        return;// Already at max
                    }
                    _addSequenceEntryRow(entries_container, slot, current_count);
                });

        // Wire remove button
        connect(remove_btn, &QPushButton::clicked, this,
                [this, entries_container, slot_name] {
                    auto children = entries_container->findChildren<QGroupBox *>(
                            Qt::FindDirectChildrenOnly);
                    if (children.size() <= 1) return;// Keep at least one

                    auto * last = children.back();
                    // Get the memory_index from the last entry
                    QLabel * idx_label = nullptr;
                    for (auto * lbl: last->findChildren<QLabel *>()) {
                        if (lbl->objectName().startsWith(QStringLiteral("seq_idx_"))) {
                            idx_label = lbl;
                            break;
                        }
                    }
                    if (idx_label) {
                        int const mem_idx = idx_label->property("memory_index").toInt();
                        auto const key = staticCacheKey(slot_name, mem_idx);
                        _assembler->clearStaticCacheEntry(key);
                        emit staticCacheChanged();
                    }
                    last->deleteLater();
                });
    } else {
        // ── Non-sequence static slot (original single-entry UI) ──
        auto * form = new QFormLayout();

        // Source data combo
        auto * source_combo = new QComboBox(group);
        source_combo->setObjectName(
                QString::fromStdString("static_source_" + slot.name));
        _populateDataSourceCombo(
                source_combo,
                SlotAssembler::dataTypeForEncoder(slot.recommended_encoder));
        form->addRow(tr("Source:"), source_combo);

        // Capture mode combo
        auto * mode_combo = new QComboBox(group);
        mode_combo->setObjectName(
                QString::fromStdString("static_mode_" + slot.name));
        mode_combo->addItem(tr("Relative"), QStringLiteral("Relative"));
        mode_combo->addItem(tr("Absolute"), QStringLiteral("Absolute"));
        form->addRow(tr("Mode:"), mode_combo);

        // Time offset (used in Relative mode)
        auto * offset_spin = new QSpinBox(group);
        offset_spin->setObjectName(
                QString::fromStdString("static_offset_" + slot.name));
        offset_spin->setRange(-99999, 0);
        offset_spin->setValue(0);
        offset_spin->setPrefix(QStringLiteral("t"));
        offset_spin->setToolTip(
                tr("Time offset for Relative mode (e.g. -1 = previous frame).\n"
                   "Ignored in Absolute mode (uses the captured frame)."));
        form->addRow(tr("Time Offset:"), offset_spin);

        layout->addLayout(form);

        // Capture button + status (for Absolute mode)
        auto * capture_row = new QHBoxLayout();
        auto * capture_btn = new QPushButton(
                tr("\u2B07 Capture Current Frame"), group);
        capture_btn->setObjectName(
                QString::fromStdString("static_capture_" + slot.name));
        capture_btn->setToolTip(
                tr("Encode data from the current frame and freeze it.\n"
                   "The cached tensor will be reused for all subsequent runs."));
        capture_btn->setEnabled(false);
        capture_row->addWidget(capture_btn);

        auto * capture_status = new QLabel(tr("Not captured"), group);
        capture_status->setObjectName(
                QString::fromStdString("static_capture_status_" + slot.name));
        capture_status->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
        capture_row->addWidget(capture_status);

        layout->addLayout(capture_row);

        // Wire capture mode switching
        auto slot_name = slot.name;
        connect(mode_combo, &QComboBox::currentTextChanged, this,
                [this, offset_spin, capture_btn, capture_status, slot_name](
                        QString const & text) {
                    bool const is_absolute = (text == tr("Absolute"));
                    offset_spin->setEnabled(!is_absolute);
                    capture_btn->setEnabled(is_absolute && _assembler->isModelReady());
                    if (!is_absolute) {
                        capture_status->setText(tr("Not captured"));
                        capture_status->setStyleSheet(
                                QStringLiteral("color: gray; font-size: 10px;"));
                    } else {
                        _updateCaptureButtonState(slot_name);
                    }
                });

        // Wire capture button
        connect(capture_btn, &QPushButton::clicked, this,
                [this, slot_name] { _onCaptureStaticInput(slot_name); });

        // Invalidate cache when data source changes
        connect(source_combo, &QComboBox::currentTextChanged, this,
                [this, slot_name, capture_status](QString const &) {
                    auto const key = staticCacheKey(slot_name, 0);
                    _assembler->clearStaticCacheEntry(key);
                    capture_status->setText(tr("Not captured"));
                    capture_status->setStyleSheet(
                            QStringLiteral("color: gray; font-size: 10px;"));
                    emit staticCacheChanged();
                });
    }

    return group;
}

// ────────────────────────────────────────────────────────────────────────────
// Sequence Entry Row
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_addSequenceEntryRow(
        QWidget * container,
        dl::TensorSlotDescriptor const & slot,
        int memory_index) {

    auto * entry_group = new QGroupBox(
            tr("Position %1").arg(memory_index), container);
    auto * form = new QFormLayout(entry_group);

    auto const slot_name = slot.name;

    // Hidden label to store the memory_index for retrieval
    auto * idx_label = new QLabel(QString::number(memory_index), entry_group);
    idx_label->setObjectName(
            QString::fromStdString("seq_idx_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    idx_label->setProperty("memory_index", memory_index);
    idx_label->setVisible(false);
    form->addRow(idx_label);

    // ── Source type selector (Static / Recurrent) ──
    auto * source_type_combo = new QComboBox(entry_group);
    source_type_combo->setObjectName(
            QString::fromStdString("seq_srctype_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    source_type_combo->addItem(tr("Static"), QStringLiteral("Static"));
    source_type_combo->addItem(tr("Recurrent"), QStringLiteral("Recurrent"));
    source_type_combo->setToolTip(
            tr("Static: capture or encode data from DataManager.\n"
               "Recurrent: fill from model output at previous frame."));
    form->addRow(tr("Source Type:"), source_type_combo);

    // ═══════════════════════════════════════════════════════════════
    // Static controls container
    // ═══════════════════════════════════════════════════════════════
    auto * static_container = new QWidget(entry_group);
    static_container->setObjectName(
            QString::fromStdString("seq_static_ctr_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    auto * static_form = new QFormLayout(static_container);
    static_form->setContentsMargins(0, 0, 0, 0);

    // Source combo
    auto * source_combo = new QComboBox(static_container);
    source_combo->setObjectName(
            QString::fromStdString("seq_source_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    _populateDataSourceCombo(
            source_combo,
            SlotAssembler::dataTypeForEncoder(slot.recommended_encoder));

    // Restore saved data_key from state
    for (auto const & si: _state->staticInputs()) {
        if (si.slot_name == slot_name && si.memory_index == memory_index) {
            int const idx = source_combo->findText(
                    QString::fromStdString(si.data_key));
            if (idx >= 0) source_combo->setCurrentIndex(idx);
            break;
        }
    }
    static_form->addRow(tr("Source:"), source_combo);

    // Capture mode combo
    auto * mode_combo = new QComboBox(static_container);
    mode_combo->setObjectName(
            QString::fromStdString("seq_mode_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    mode_combo->addItem(tr("Relative"), QStringLiteral("Relative"));
    mode_combo->addItem(tr("Absolute"), QStringLiteral("Absolute"));

    // Restore saved mode
    for (auto const & si: _state->staticInputs()) {
        if (si.slot_name == slot_name && si.memory_index == memory_index) {
            int const idx = mode_combo->findData(
                    QString::fromStdString(si.capture_mode_str));
            if (idx >= 0) mode_combo->setCurrentIndex(idx);
            break;
        }
    }
    static_form->addRow(tr("Mode:"), mode_combo);

    // Time offset
    auto * offset_spin = new QSpinBox(static_container);
    offset_spin->setObjectName(
            QString::fromStdString("seq_offset_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    offset_spin->setRange(-99999, 0);
    offset_spin->setPrefix(QStringLiteral("t"));
    offset_spin->setToolTip(
            tr("Time offset for Relative mode."));

    // Restore saved offset
    for (auto const & si: _state->staticInputs()) {
        if (si.slot_name == slot_name && si.memory_index == memory_index) {
            offset_spin->setValue(si.time_offset);
            break;
        }
    }
    static_form->addRow(tr("Offset:"), offset_spin);

    // Capture button + status
    auto * capture_row = new QHBoxLayout();
    auto * capture_btn = new QPushButton(
            tr("\u2B07 Capture"), static_container);
    capture_btn->setObjectName(
            QString::fromStdString("seq_capture_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    capture_btn->setEnabled(false);
    capture_row->addWidget(capture_btn);

    auto * capture_status = new QLabel(tr("Not captured"), static_container);
    capture_status->setObjectName(
            QString::fromStdString("seq_status_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    capture_status->setStyleSheet(
            QStringLiteral("color: gray; font-size: 10px;"));

    // Restore captured state
    for (auto const & si: _state->staticInputs()) {
        if (si.slot_name == slot_name && si.memory_index == memory_index) {
            auto const key = staticCacheKey(slot_name, memory_index);
            if (_assembler->hasStaticCacheEntry(key) && si.captured_frame >= 0) {
                capture_status->setText(
                        tr("\u2713 Frame %1").arg(si.captured_frame));
                capture_status->setStyleSheet(
                        QStringLiteral("color: green; font-size: 10px;"));
            }
            break;
        }
    }
    capture_row->addWidget(capture_status);
    static_form->addRow(capture_row);

    form->addRow(static_container);

    // ═══════════════════════════════════════════════════════════════
    // Recurrent controls container
    // ═══════════════════════════════════════════════════════════════
    auto * recurrent_container = new QWidget(entry_group);
    recurrent_container->setObjectName(
            QString::fromStdString("seq_recur_ctr_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    auto * recur_form = new QFormLayout(recurrent_container);
    recur_form->setContentsMargins(0, 0, 0, 0);

    // Output slot combo
    auto * output_combo = new QComboBox(recurrent_container);
    output_combo->setObjectName(
            QString::fromStdString("seq_recur_output_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    output_combo->addItem(tr("(None)"), QString{});
    if (_current_info) {
        for (auto const & out: _current_info->outputs) {
            output_combo->addItem(
                    QString::fromStdString(out.name),
                    QString::fromStdString(out.name));
        }
    }

    // Restore from state
    for (auto const & rb: _state->recurrentBindings()) {
        if (rb.input_slot_name == slot_name &&
            rb.target_memory_index == memory_index) {
            int const idx = output_combo->findData(
                    QString::fromStdString(rb.output_slot_name));
            if (idx >= 0) output_combo->setCurrentIndex(idx);
            break;
        }
    }
    recur_form->addRow(tr("Output slot:"), output_combo);

    // Init mode combo
    auto * init_combo = new QComboBox(recurrent_container);
    init_combo->setObjectName(
            QString::fromStdString("seq_recur_init_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    init_combo->addItem(tr("Zeros"), QStringLiteral("Zeros"));
    init_combo->addItem(tr("Static Capture"), QStringLiteral("StaticCapture"));
    init_combo->addItem(tr("First Output"), QStringLiteral("FirstOutput"));
    init_combo->setToolTip(
            tr("How to initialize this position at t=0:\n"
               "• Zeros: all-zeros tensor\n"
               "• Static Capture: use a previously captured tensor\n"
               "• First Output: run model once with zeros, use output"));

    // Restore from state
    for (auto const & rb: _state->recurrentBindings()) {
        if (rb.input_slot_name == slot_name &&
            rb.target_memory_index == memory_index) {
            int const idx = init_combo->findData(
                    QString::fromStdString(rb.init_mode_str));
            if (idx >= 0) init_combo->setCurrentIndex(idx);
            break;
        }
    }
    recur_form->addRow(tr("Init mode:"), init_combo);

    // Recurrent status label
    auto * recur_status = new QLabel(recurrent_container);
    recur_status->setObjectName(
            QString::fromStdString("seq_recur_status_" + slot_name + "_" +
                                   std::to_string(memory_index)));
    recur_status->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    recur_form->addRow(recur_status);

    form->addRow(recurrent_container);

    // ═══════════════════════════════════════════════════════════════
    // Source type switching logic
    // ═══════════════════════════════════════════════════════════════

    // Determine initial source type from state
    bool is_recurrent = false;
    for (auto const & rb: _state->recurrentBindings()) {
        if (rb.input_slot_name == slot_name &&
            rb.target_memory_index == memory_index) {
            is_recurrent = true;
            break;
        }
    }
    if (is_recurrent) {
        source_type_combo->setCurrentIndex(
                source_type_combo->findData(QStringLiteral("Recurrent")));
    }

    auto updateVisibility = [static_container, recurrent_container,
                             source_type_combo] {
        bool const is_static =
                source_type_combo->currentData().toString() == QStringLiteral("Static");
        static_container->setVisible(is_static);
        recurrent_container->setVisible(!is_static);
    };
    updateVisibility();

    connect(source_type_combo, &QComboBox::currentIndexChanged, this,
            [updateVisibility, this](int) {
                updateVisibility();
                _updateBatchSizeConstraint();
            });

    // Update recurrent status when output changes
    auto updateRecurStatus = [output_combo, recur_status, slot_name,
                              memory_index] {
        auto const out_name = output_combo->currentData().toString().toStdString();
        if (out_name.empty()) {
            recur_status->setText(tr("No feedback configured"));
        } else {
            recur_status->setText(
                    tr("Feedback: %1 \u2192 %2[%3]")
                            .arg(QString::fromStdString(out_name))
                            .arg(QString::fromStdString(slot_name))
                            .arg(memory_index));
        }
    };
    updateRecurStatus();
    connect(output_combo, &QComboBox::currentIndexChanged, this,
            [updateRecurStatus](int) { updateRecurStatus(); });

    // ═══════════════════════════════════════════════════════════════
    // Wire static controls (same as before)
    // ═══════════════════════════════════════════════════════════════

    // Wire mode switching
    connect(mode_combo, &QComboBox::currentTextChanged, this,
            [offset_spin, capture_btn, capture_status, this](
                    QString const & text) {
                bool const is_absolute = (text == tr("Absolute"));
                offset_spin->setEnabled(!is_absolute);
                capture_btn->setEnabled(is_absolute && _assembler->isModelReady());
                if (!is_absolute) {
                    capture_status->setText(tr("Not captured"));
                    capture_status->setStyleSheet(
                            QStringLiteral("color: gray; font-size: 10px;"));
                }
            });

    // Wire capture button — captures this specific sequence entry
    connect(capture_btn, &QPushButton::clicked, this,
            [this, slot_name, memory_index, source_combo, mode_combo,
             offset_spin, capture_status] {
                _onCaptureSequenceEntry(
                        slot_name, memory_index, source_combo, mode_combo,
                        offset_spin, capture_status);
            });

    // Invalidate cache on source change
    connect(source_combo, &QComboBox::currentTextChanged, this,
            [this, slot_name, memory_index, capture_status](QString const &) {
                auto const key = staticCacheKey(slot_name, memory_index);
                _assembler->clearStaticCacheEntry(key);
                capture_status->setText(tr("Not captured"));
                capture_status->setStyleSheet(
                        QStringLiteral("color: gray; font-size: 10px;"));
                emit staticCacheChanged();
            });

    auto * container_layout = qobject_cast<QVBoxLayout *>(container->layout());
    if (container_layout) {
        container_layout->addWidget(entry_group);
    }
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
    for (auto const & dec: SlotAssembler::availableDecoders()) {
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

    for (auto const & key: keys) {
        combo->addItem(QString::fromStdString(key));
    }

    int const idx = combo->findText(current);
    if (idx >= 0) combo->setCurrentIndex(idx);
}

void DeepLearningPropertiesWidget::_refreshDataSourceCombos() {
    if (!_current_info || !_dynamic_container) return;

    // Refresh dynamic input source combos
    for (auto const & slot: _current_info->inputs) {
        if (!slot.is_static && !slot.is_boolean_mask) {
            auto * source = _dynamic_container->findChild<QComboBox *>(
                    QString::fromStdString("source_" + slot.name));
            auto * encoder = _dynamic_container->findChild<QComboBox *>(
                    QString::fromStdString("encoder_" + slot.name));
            if (source) {
                std::string const enc_id = encoder
                                                   ? encoder->currentText().toStdString()
                                                   : slot.recommended_encoder;
                _populateDataSourceCombo(
                        source, SlotAssembler::dataTypeForEncoder(enc_id));
            }
        } else if (slot.is_static && !slot.is_boolean_mask) {
            if (slot.hasSequenceDim()) {
                // Refresh all sequence entry source combos
                auto const prefix = QString::fromStdString(
                        "seq_source_" + slot.name + "_");
                for (auto * combo: _dynamic_container->findChildren<QComboBox *>()) {
                    if (combo->objectName().startsWith(prefix)) {
                        _populateDataSourceCombo(
                                combo,
                                SlotAssembler::dataTypeForEncoder(
                                        slot.recommended_encoder));
                    }
                }
            } else {
                auto * source = _dynamic_container->findChild<QComboBox *>(
                        QString::fromStdString("static_source_" + slot.name));
                if (source) {
                    _populateDataSourceCombo(
                            source,
                            SlotAssembler::dataTypeForEncoder(
                                    slot.recommended_encoder));
                }
            }
        }
    }

    // Refresh output target combos
    for (auto const & slot: _current_info->outputs) {
        auto * target = _dynamic_container->findChild<QComboBox *>(
                QString::fromStdString("target_" + slot.name));
        if (target) {
            _populateDataSourceCombo(
                    target,
                    SlotAssembler::dataTypeForDecoder(
                            slot.recommended_decoder));
        }
    }
}

std::vector<std::string> DeepLearningPropertiesWidget::_modesForEncoder(
        std::string const & encoder_id) {

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
    for (auto const & slot: _current_info->inputs) {
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
        auto * time_offset = _dynamic_container->findChild<QSpinBox *>(
                QString::fromStdString("time_offset_" + slot.name));

        if (source) binding.data_key = source->currentText().toStdString();
        if (encoder) binding.encoder_id = encoder->currentText().toStdString();
        if (mode) binding.mode = mode->currentText().toStdString();
        if (sigma) binding.gaussian_sigma = static_cast<float>(sigma->value());
        if (time_offset) binding.time_offset = time_offset->value();

        if (!binding.data_key.empty() && binding.data_key != "(None)") {
            input_bindings.push_back(std::move(binding));
        }
    }
    _state->setInputBindings(std::move(input_bindings));

    // ── Output bindings ──
    std::vector<OutputBindingData> output_bindings;
    for (auto const & slot: _current_info->outputs) {
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
    // Hybrid recurrent bindings collected from sequence entries
    std::vector<RecurrentBindingData> hybrid_recurrent_bindings;

    for (auto const & slot: _current_info->inputs) {
        if (!slot.is_static || slot.is_boolean_mask) continue;

        if (slot.hasSequenceDim()) {
            // Sequence slot: collect entries from all sequence rows
            auto * entries_container = _dynamic_container->findChild<QWidget *>(
                    QString::fromStdString("seq_entries_" + slot.name));
            if (!entries_container) continue;

            auto entry_groups = entries_container->findChildren<QGroupBox *>(
                    Qt::FindDirectChildrenOnly);

            for (auto * entry_group: entry_groups) {
                // Find the memory_index from the hidden label
                QLabel * idx_lbl = nullptr;
                for (auto * lbl: entry_group->findChildren<QLabel *>()) {
                    if (lbl->objectName().startsWith(QStringLiteral("seq_idx_"))) {
                        idx_lbl = lbl;
                        break;
                    }
                }
                if (!idx_lbl) continue;
                int const mem_idx = idx_lbl->property("memory_index").toInt();

                auto const suffix = slot.name + "_" + std::to_string(mem_idx);

                // Check source type
                auto * srctype = entry_group->findChild<QComboBox *>(
                        QString::fromStdString("seq_srctype_" + suffix));
                bool const is_recurrent = srctype &&
                                          srctype->currentData().toString() == QStringLiteral("Recurrent");

                if (is_recurrent) {
                    // Collect as a hybrid recurrent binding
                    auto * output = entry_group->findChild<QComboBox *>(
                            QString::fromStdString("seq_recur_output_" + suffix));
                    auto * init = entry_group->findChild<QComboBox *>(
                            QString::fromStdString("seq_recur_init_" + suffix));

                    if (!output || !init) continue;
                    auto const out_name = output->currentData().toString().toStdString();
                    if (out_name.empty()) continue;

                    RecurrentBindingData rb;
                    rb.input_slot_name = slot.name;
                    rb.output_slot_name = out_name;
                    rb.target_memory_index = mem_idx;
                    rb.init_mode_str = init->currentData().toString().toStdString();
                    hybrid_recurrent_bindings.push_back(std::move(rb));
                } else {
                    // Collect as a static input (existing behavior)
                    auto * source = entry_group->findChild<QComboBox *>(
                            QString::fromStdString("seq_source_" + suffix));
                    auto * mode = entry_group->findChild<QComboBox *>(
                            QString::fromStdString("seq_mode_" + suffix));
                    auto * offset = entry_group->findChild<QSpinBox *>(
                            QString::fromStdString("seq_offset_" + suffix));

                    StaticInputData si;
                    si.slot_name = slot.name;
                    si.memory_index = mem_idx;
                    si.active = true;

                    if (source) si.data_key = source->currentText().toStdString();
                    if (offset) si.time_offset = offset->value();
                    if (mode) {
                        si.capture_mode_str =
                                mode->currentData().toString().toStdString();
                    }

                    // Preserve captured_frame from existing state
                    for (auto const & prev: _state->staticInputs()) {
                        if (prev.slot_name == slot.name &&
                            prev.memory_index == mem_idx) {
                            si.captured_frame = prev.captured_frame;
                            break;
                        }
                    }

                    if (!si.data_key.empty() && si.data_key != "(None)") {
                        static_inputs.push_back(std::move(si));
                    }
                }
            }
        } else {
            // Non-sequence slot: single entry (original behavior)
            StaticInputData si;
            si.slot_name = slot.name;
            si.memory_index = 0;

            auto * source = _dynamic_container->findChild<QComboBox *>(
                    QString::fromStdString("static_source_" + slot.name));
            auto * offset = _dynamic_container->findChild<QSpinBox *>(
                    QString::fromStdString("static_offset_" + slot.name));
            auto * mode = _dynamic_container->findChild<QComboBox *>(
                    QString::fromStdString("static_mode_" + slot.name));

            if (source) si.data_key = source->currentText().toStdString();
            if (offset) si.time_offset = offset->value();
            if (mode) {
                si.capture_mode_str =
                        mode->currentData().toString().toStdString();
            }

            // Preserve captured_frame from existing state
            for (auto const & prev: _state->staticInputs()) {
                if (prev.slot_name == slot.name) {
                    si.captured_frame = prev.captured_frame;
                    break;
                }
            }

            if (!si.data_key.empty() && si.data_key != "(None)") {
                static_inputs.push_back(std::move(si));
            }
        }
    }
    _state->setStaticInputs(std::move(static_inputs));

    // ── Recurrent bindings ──
    // Start with hybrid recurrent bindings collected from sequence entries
    std::vector<RecurrentBindingData> recurrent_bindings =
            std::move(hybrid_recurrent_bindings);

    // Add whole-slot recurrent bindings from non-sequence static inputs
    for (auto const & slot: _current_info->inputs) {
        if (!slot.is_static || slot.is_boolean_mask) continue;
        // Sequence slots use per-position hybrid bindings (collected above)
        if (slot.hasSequenceDim()) continue;

        auto * output_combo = _dynamic_container->findChild<QComboBox *>(
                QString::fromStdString("recurrent_output_" + slot.name));
        auto * init_combo = _dynamic_container->findChild<QComboBox *>(
                QString::fromStdString("recurrent_init_" + slot.name));

        if (!output_combo || !init_combo) continue;

        auto const output_name = output_combo->currentData().toString().toStdString();
        if (output_name.empty()) continue;// "(None)" or empty

        RecurrentBindingData rb;
        rb.input_slot_name = slot.name;
        rb.output_slot_name = output_name;
        rb.init_mode_str = init_combo->currentData().toString().toStdString();

        // If StaticCapture, pick up init_data_key and init_frame from
        // any existing static input for this slot
        if (rb.initMode() == RecurrentInitMode::StaticCapture) {
            for (auto const & si: _state->staticInputs()) {
                if (si.slot_name == slot.name) {
                    rb.init_data_key = si.data_key;
                    rb.init_frame = si.captured_frame;
                    break;
                }
            }
        }

        recurrent_bindings.push_back(std::move(rb));
    }
    _state->setRecurrentBindings(std::move(recurrent_bindings));
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
        for (auto const & binding: _state->inputBindings()) {
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

// ────────────────────────────────────────────────────────────────────────────
// Static Input Capture
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_onCaptureStaticInput(
        std::string const & slot_name) {

    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    _syncBindingsFromUi();

    // Find the static input entry for this slot
    auto const & static_inputs = _state->staticInputs();
    StaticInputData const * entry = nullptr;
    for (auto const & si: static_inputs) {
        if (si.slot_name == slot_name) {
            entry = &si;
            break;
        }
    }
    if (!entry || entry->data_key.empty()) {
        QMessageBox::warning(this, tr("No Source"),
                             tr("Select a data source for this slot first."));
        return;
    }

    // Determine which frame to capture
    int frame = _state->currentFrame();
    if (_current_time_position.has_value()) {
        frame = static_cast<int>(_current_time_position->index.getValue());
    }

    // Determine source image size
    ImageSize source_size{256, 256};
    for (auto const & binding: _state->inputBindings()) {
        auto media = _data_manager->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    bool const ok = _assembler->captureStaticInput(
            *_data_manager, *entry, frame, source_size);

    // Update the captured_frame in state
    if (ok) {
        auto inputs = _state->staticInputs();
        for (auto & si: inputs) {
            if (si.slot_name == slot_name && si.memory_index == entry->memory_index) {
                si.captured_frame = frame;
                break;
            }
        }
        _state->setStaticInputs(std::move(inputs));
    }

    _updateCaptureButtonState(slot_name);
    emit staticCacheChanged();
}

void DeepLearningPropertiesWidget::_onCaptureSequenceEntry(
        std::string const & slot_name,
        int memory_index,
        QComboBox * source_combo,
        QComboBox * mode_combo,
        QSpinBox * offset_spin,
        QLabel * capture_status) {

    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    auto const data_key = source_combo->currentText().toStdString();
    if (data_key.empty() || data_key == "(None)") {
        QMessageBox::warning(this, tr("No Source"),
                             tr("Select a data source for this entry first."));
        return;
    }

    // Build a StaticInputData for this entry
    StaticInputData entry;
    entry.slot_name = slot_name;
    entry.memory_index = memory_index;
    entry.data_key = data_key;
    entry.time_offset = offset_spin->value();
    entry.capture_mode_str = mode_combo->currentData().toString().toStdString();
    entry.active = true;

    // Determine which frame to capture
    int frame = _state->currentFrame();
    if (_current_time_position.has_value()) {
        frame = static_cast<int>(_current_time_position->index.getValue());
    }

    // Determine source image size
    ImageSize source_size{256, 256};
    for (auto const & binding: _state->inputBindings()) {
        auto media = _data_manager->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    bool const ok = _assembler->captureStaticInput(
            *_data_manager, entry, frame, source_size);

    if (ok) {
        capture_status->setText(
                tr("\u2713 Frame %1").arg(frame));
        capture_status->setStyleSheet(
                QStringLiteral("color: green; font-size: 10px;"));

        // Update captured_frame in state
        _syncBindingsFromUi();
        auto inputs = _state->staticInputs();
        for (auto & si: inputs) {
            if (si.slot_name == slot_name && si.memory_index == memory_index) {
                si.captured_frame = frame;
                break;
            }
        }
        _state->setStaticInputs(std::move(inputs));
    } else {
        capture_status->setText(tr("\u2717 Capture failed"));
        capture_status->setStyleSheet(
                QStringLiteral("color: red; font-size: 10px;"));
    }

    emit staticCacheChanged();
}

void DeepLearningPropertiesWidget::_updateCaptureButtonState(
        std::string const & slot_name) {

    auto * status = _dynamic_container->findChild<QLabel *>(
            QString::fromStdString("static_capture_status_" + slot_name));
    if (!status) return;

    auto const key = staticCacheKey(slot_name, 0);
    if (_assembler->hasStaticCacheEntry(key)) {
        auto range = _assembler->staticCacheTensorRange(key);
        auto shape = _assembler->staticCacheTensorShape(key);

        // Find which frame was captured
        int captured_frame = -1;
        for (auto const & si: _state->staticInputs()) {
            if (si.slot_name == slot_name) {
                captured_frame = si.captured_frame;
                break;
            }
        }

        QString info = tr("\u2713 Captured");
        if (captured_frame >= 0) {
            info += tr(" (frame %1)").arg(captured_frame);
        }
        info += tr(" range [%1, %2]")
                        .arg(static_cast<double>(range.first), 0, 'f', 2)
                        .arg(static_cast<double>(range.second), 0, 'f', 2);
        status->setText(info);
        status->setStyleSheet(
                QStringLiteral("color: green; font-size: 10px;"));
    } else {
        status->setText(tr("Not captured"));
        status->setStyleSheet(
                QStringLiteral("color: gray; font-size: 10px;"));
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Recurrent Input Panel
// ────────────────────────────────────────────────────────────────────────────

QGroupBox * DeepLearningPropertiesWidget::_buildRecurrentInputGroup(
        dl::TensorSlotDescriptor const & input_slot,
        std::vector<dl::TensorSlotDescriptor> const & output_slots) {

    auto * group = new QGroupBox(
            QString::fromStdString(input_slot.name) + tr(" (recurrent)"),
            _dynamic_container);
    auto * form = new QFormLayout(group);

    auto * info = new QLabel(
            tr("Map a model output to this input slot.\n"
               "The output at frame t becomes the input at frame t+1."),
            group);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    form->addRow(info);

    // Output slot selector
    auto * output_combo = new QComboBox(group);
    output_combo->setObjectName(
            QString::fromStdString("recurrent_output_" + input_slot.name));
    output_combo->addItem(tr("(None)"), QString{});
    for (auto const & out: output_slots) {
        output_combo->addItem(
                QString::fromStdString(out.name),
                QString::fromStdString(out.name));
    }

    // Restore from state
    for (auto const & rb: _state->recurrentBindings()) {
        if (rb.input_slot_name == input_slot.name) {
            int const idx = output_combo->findData(
                    QString::fromStdString(rb.output_slot_name));
            if (idx >= 0) output_combo->setCurrentIndex(idx);
            break;
        }
    }
    form->addRow(tr("Output slot:"), output_combo);

    // Initialization mode
    auto * init_combo = new QComboBox(group);
    init_combo->setObjectName(
            QString::fromStdString("recurrent_init_" + input_slot.name));
    init_combo->addItem(tr("Zeros"), QStringLiteral("Zeros"));
    init_combo->addItem(tr("Static Capture"), QStringLiteral("StaticCapture"));
    init_combo->addItem(tr("First Output"), QStringLiteral("FirstOutput"));
    init_combo->setToolTip(
            tr("How to initialize this input at t=0:\n"
               "• Zeros: all-zeros tensor\n"
               "• Static Capture: use a previously captured tensor\n"
               "• First Output: run model once with zeros, use output as init"));

    // Restore from state
    for (auto const & rb: _state->recurrentBindings()) {
        if (rb.input_slot_name == input_slot.name) {
            int const idx = init_combo->findData(
                    QString::fromStdString(rb.init_mode_str));
            if (idx >= 0) init_combo->setCurrentIndex(idx);
            break;
        }
    }
    form->addRow(tr("Init mode:"), init_combo);

    // Status label
    auto * status = new QLabel(group);
    status->setObjectName(
            QString::fromStdString("recurrent_status_" + input_slot.name));
    status->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));

    auto const slot_name = input_slot.name;
    auto updateStatus = [this, output_combo, status, slot_name] {
        auto const out_name = output_combo->currentData().toString().toStdString();
        if (out_name.empty()) {
            status->setText(tr("No feedback configured"));
        } else {
            status->setText(
                    tr("Feedback: %1 \u2192 %2")
                            .arg(QString::fromStdString(out_name))
                            .arg(QString::fromStdString(slot_name)));
        }
        _updateBatchSizeConstraint();
    };
    updateStatus();
    form->addRow(status);

    connect(output_combo, &QComboBox::currentIndexChanged, this,
            [updateStatus](int) { updateStatus(); });
    connect(init_combo, &QComboBox::currentIndexChanged, this,
            [updateStatus](int) { updateStatus(); });

    return group;
}

// ────────────────────────────────────────────────────────────────────────────
// Batch Size Constraint
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_updateBatchSizeConstraint() {
    if (!_dynamic_container || !_batch_size_spin || !_current_info) return;

    // ── Step 1: Check model-level batch mode ──
    auto const & mode = _current_info->batch_mode;
    bool const model_locked = dl::isBatchLocked(mode);
    int const model_max = dl::maxBatchSizeFromMode(mode);
    int const model_min = dl::minBatchSizeFromMode(mode);

    // ── Step 2: Check if any recurrent bindings are active in the UI ──
    bool has_recurrent = false;
    for (auto const & slot: _current_info->inputs) {
        if (!slot.is_static || slot.is_boolean_mask) continue;

        if (slot.hasSequenceDim()) {
            auto const prefix = QString::fromStdString(
                    "seq_srctype_" + slot.name + "_");
            for (auto * combo: _dynamic_container->findChildren<QComboBox *>()) {
                if (combo->objectName().startsWith(prefix)) {
                    if (combo->currentData().toString() == QStringLiteral("Recurrent")) {
                        has_recurrent = true;
                        break;
                    }
                }
            }
        } else {
            auto * output_combo = _dynamic_container->findChild<QComboBox *>(
                    QString::fromStdString("recurrent_output_" + slot.name));
            if (output_combo) {
                auto const out_name = output_combo->currentData().toString().toStdString();
                if (!out_name.empty()) {
                    has_recurrent = true;
                }
            }
        }
        if (has_recurrent) break;
    }

    // ── Step 3: Determine the most restrictive constraint ──
    // Priority: recurrent bindings → model RecurrentOnly → model Fixed → model Dynamic
    bool lock_to_one = has_recurrent || model_locked;
    QString tooltip;

    if (has_recurrent) {
        tooltip = tr("Batch size is locked to 1 when recurrent bindings are active.\n"
                     "Sequential frame-by-frame processing requires batch=1.");
    } else if (std::holds_alternative<dl::RecurrentOnlyBatch>(mode)) {
        tooltip = tr("Model requires batch size = 1 (recurrent-only mode).");
    } else if (auto const * f = std::get_if<dl::FixedBatch>(&mode)) {
        if (f->size == 1) {
            tooltip = tr("Model is compiled for fixed batch size = 1.");
        }
    }

    if (lock_to_one) {
        _batch_size_spin->setValue(1);
        _batch_size_spin->setEnabled(false);
        _batch_size_spin->setToolTip(tooltip);
    } else if (auto const * f = std::get_if<dl::FixedBatch>(&mode)) {
        // Fixed batch: lock spinbox to that value
        _batch_size_spin->setRange(f->size, f->size);
        _batch_size_spin->setValue(f->size);
        _batch_size_spin->setEnabled(false);
        _batch_size_spin->setToolTip(
                tr("Model is compiled for fixed batch size = %1.")
                        .arg(f->size));
    } else {
        // Dynamic batch: allow range
        _batch_size_spin->setEnabled(true);
        _batch_size_spin->setMinimum(model_min > 0 ? model_min : 1);
        if (model_max > 0) {
            _batch_size_spin->setMaximum(model_max);
        } else {
            _batch_size_spin->setMaximum(9999);
        }
        auto const & bm = std::get<dl::DynamicBatch>(mode);
        tooltip = tr("Batch mode: Dynamic(%1, %2)")
                          .arg(bm.min_size)
                          .arg(bm.max_size > 0 ? QString::number(bm.max_size)
                                               : tr("unlimited"));
        _batch_size_spin->setToolTip(tooltip);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Run Recurrent Sequence
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_onRunRecurrentSequence() {
    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    _syncBindingsFromUi();

    auto const & recurrent = _state->recurrentBindings();
    if (recurrent.empty()) {
        QMessageBox::warning(this, tr("No Recurrent Bindings"),
                             tr("Configure at least one recurrent feedback binding\n"
                                "by selecting an output slot in the Recurrent Inputs section."));
        return;
    }

    int const start_frame = _state->currentFrame();
    int const frame_count = _state->batchSize() > 1 ? _state->batchSize() : _batch_size_spin->maximum();

    // Ask user for frame count
    auto * frame_count_spin = new QSpinBox(this);
    frame_count_spin->setRange(1, 999999);
    frame_count_spin->setValue(std::min(100, frame_count));

    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("Recurrent Inference"));
    dialog.setText(tr("Run recurrent inference starting at frame %1.\n"
                      "How many frames to process?")
                           .arg(start_frame));
    dialog.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

    auto * layout = qobject_cast<QGridLayout *>(dialog.layout());
    if (layout) {
        layout->addWidget(new QLabel(tr("Frame count:"), &dialog), 2, 0);
        layout->addWidget(frame_count_spin, 2, 1);
    }

    if (dialog.exec() != QMessageBox::Ok) return;

    int const count = frame_count_spin->value();

    // Determine source image size
    ImageSize source_size{256, 256};
    for (auto const & binding: _state->inputBindings()) {
        auto media = _data_manager->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    try {
        // Clear previous recurrent state
        _assembler->clearRecurrentCache();

        _assembler->runRecurrentSequence(
                *_data_manager,
                _state->inputBindings(),
                _state->staticInputs(),
                _state->outputBindings(),
                recurrent,
                start_frame,
                count,
                source_size,
                [this](int current, int total) {
                    emit recurrentProgressChanged(current, total);
                });

        _weights_status_label->setText(
                tr("\u2713 Recurrent inference complete (%1 frames from %2)")
                        .arg(count)
                        .arg(start_frame));
        _weights_status_label->setStyleSheet(QStringLiteral("color: green;"));

    } catch (std::exception const & e) {
        QMessageBox::critical(
                this, tr("Inference Error"),
                tr("Recurrent inference failed:\n%1")
                        .arg(QString::fromUtf8(e.what())));
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Time Position Tracking
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::onTimeChanged(TimePosition const & position) {
    _current_time_position = position;

    // Enable/disable the predict current frame button based on whether we have
    // a valid time position and the model is ready
    if (_predict_current_frame_btn) {
        bool const ready = _assembler->isModelReady();
        _predict_current_frame_btn->setEnabled(ready && _current_time_position.has_value());
    }
}

void DeepLearningPropertiesWidget::_onPredictCurrentFrame() {
    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    if (!_current_time_position.has_value()) {
        QMessageBox::warning(this, tr("No Time Position"),
                             tr("No current time position available from timeline."));
        return;
    }

    _syncBindingsFromUi();

    try {
        // Get the frame index from the current time position
        // The time position already has the index in the TimeFrame
        int const frame = static_cast<int>(_current_time_position->index.getValue());

        // Determine source image size from primary media binding
        ImageSize source_size{256, 256};
        for (auto const & binding: _state->inputBindings()) {
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
                tr("\u2713 Inference complete (current frame %1)").arg(frame));
        _weights_status_label->setStyleSheet(QStringLiteral("color: green;"));

    } catch (std::exception const & e) {
        QMessageBox::critical(
                this, tr("Inference Error"),
                tr("Forward pass failed:\n%1").arg(QString::fromUtf8(e.what())));
    }
}
