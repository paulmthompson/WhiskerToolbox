#include "DeepLearningPropertiesWidget.hpp"

#include "DeepLearning_Widget/Core/BindingConversion.hpp"
#include "DeepLearning_Widget/Core/ConstraintEnforcer.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DynamicInputSlotWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/EncoderShapeWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/OutputSlotWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/PostEncoderWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/RecurrentBindingWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/SequenceSlotWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/StaticInputSlotWidget.hpp"
#include "DeepLearning_Widget/UI/InferenceController.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
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
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "EditorState/StrongTypes.hpp"
#include "KeymapSystem/KeyActionAdapter.hpp"
#include "KeymapSystem/KeymapManager.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <variant>

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
      _assembler(std::make_unique<SlotAssembler>()),
      _inference_controller(std::make_unique<InferenceController>(
              _assembler.get(), _data_manager, _state, this)) {

    _buildUi();
    _populateModelCombo();

    connect(_inference_controller.get(),
            &InferenceController::batchProgressChanged,
            this,
            &DeepLearningPropertiesWidget::batchProgressChanged);
    connect(_inference_controller.get(),
            &InferenceController::recurrentProgressChanged,
            this,
            &DeepLearningPropertiesWidget::recurrentProgressChanged);
    connect(_inference_controller.get(),
            &InferenceController::batchFinished,
            this,
            &DeepLearningPropertiesWidget::_onInferenceBatchFinished);
    connect(_inference_controller.get(),
            &InferenceController::runningChanged,
            this,
            &DeepLearningPropertiesWidget::_onInferenceRunningChanged);

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

    connect(_state.get(), &DeepLearningState::postEncoderModuleChanged, this,
            [this] {
                if (_post_encoder_widget) {
                    _post_encoder_widget->refreshDataSources();
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

void DeepLearningPropertiesWidget::setKeymapManager(KeymapSystem::KeymapManager * manager) {
    if (!manager) {
        return;
    }

    _key_adapter = new KeymapSystem::KeyActionAdapter(this);
    _key_adapter->setTypeId(EditorLib::EditorTypeId(QStringLiteral("DeepLearningWidget")));

    _key_adapter->setHandler([this](QString const & action_id) -> bool {
        if (action_id == QStringLiteral("deep_learning.predict_current")) {
            _onPredictCurrentFrame();
            return true;
        }
        return false;
    });

    manager->registerAdapter(_key_adapter);
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

    // ── Device (only shown when GPU is available) ──
    if (SlotAssembler::isCudaAvailable()) {
        auto * group = new QGroupBox(tr("Device"), this);
        auto * form = new QFormLayout(group);

        _device_combo = new QComboBox(group);
        _device_combo->addItem(tr("GPU (CUDA)"), QStringLiteral("cuda"));
        _device_combo->addItem(tr("CPU"), QStringLiteral("cpu"));

        // Default to GPU (index 0) since CUDA is available
        auto const dev_name = SlotAssembler::currentDeviceName();
        _device_combo->setCurrentIndex(
                dev_name.find("CUDA") != std::string::npos ? 0 : 1);

        form->addRow(tr("Device:"), _device_combo);
        main_layout->addWidget(group);

        connect(_device_combo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DeepLearningPropertiesWidget::_onDeviceComboChanged);
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

    _weights_validation_error.clear();

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

        // Apply saved encoder shape if configured
        if (_current_info && model_id == "general_encoder") {
            dl::widget::EncoderShapeWidget::applyEncoderShapeToAssembler(
                    _state.get(), _assembler.get());
            _current_info = _assembler->currentModelDisplayInfo();
        }
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

void DeepLearningPropertiesWidget::_onDeviceComboChanged(int index) {
    if (!_device_combo) {
        return;
    }
    auto const key = _device_combo->itemData(index).toString().toStdString();
    SlotAssembler::setDeviceByName(key);
}

void DeepLearningPropertiesWidget::_loadModelIfReady() {
    _updateWeightsStatus();

    // Gate weight loading on shape being explicitly configured for
    // general_encoder (shape fields default to 0 until Apply is clicked).
    if (_assembler->currentModelId() == "general_encoder" &&
        !_state->shapeConfigured()) {
        return;
    }

    _weights_validation_error.clear();

    auto const & path = _state->weightsPath();
    if (!path.empty() && !_assembler->currentModelId().empty()) {
        try {
            if (std::filesystem::exists(path)) {
                _assembler->loadWeights(path);
                _weights_validation_error = _assembler->validateWeights();
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

    // Notify all static input slot widgets of model readiness (enables capture button)
    for (auto * slot_widget: _static_input_widgets) {
        slot_widget->setModelReady(ready);
    }
    for (auto * seq_widget: _sequence_slot_widgets) {
        seq_widget->setModelReady(ready);
    }
}

void DeepLearningPropertiesWidget::_updateWeightsStatus() {
    if (_assembler->currentModelId().empty()) {
        _weights_status_label->setText(tr("No model selected"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: gray;"));
        return;
    }

    // For general_encoder, require the user to explicitly apply a shape
    // before enabling weight loading — mismatched shapes cause silent errors.
    if (_assembler->currentModelId() == "general_encoder" &&
        !_state->shapeConfigured()) {
        _weights_path_edit->setEnabled(false);
        _weights_browse_btn->setEnabled(false);
        _weights_status_label->setText(tr("Set encoder shape first"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: orange;"));
        return;
    }
    _weights_path_edit->setEnabled(true);
    _weights_browse_btn->setEnabled(true);

    auto const & path = _state->weightsPath();
    if (path.empty()) {
        _weights_status_label->setText(tr("No weights file specified"));
        _weights_status_label->setStyleSheet(
                QStringLiteral("color: orange;"));
    } else if (!std::filesystem::exists(path)) {
        _weights_status_label->setText(tr("\u2717 File not found"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: red;"));
    } else if (_assembler->isModelReady()) {
        if (_weights_validation_error.empty()) {
            _weights_status_label->setText(tr("\u2713 Loaded & validated"));
            _weights_status_label->setStyleSheet(
                    QStringLiteral("color: green;"));
        } else {
            _weights_status_label->setText(
                    tr("\u26a0 Shape mismatch: %1")
                            .arg(QString::fromStdString(_weights_validation_error)));
            _weights_status_label->setStyleSheet(
                    QStringLiteral("color: orange;"));
        }
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
    _dynamic_input_widgets.clear();
    _static_input_widgets.clear();
    _sequence_slot_widgets.clear();
    _output_slot_widgets.clear();
    _post_encoder_widget = nullptr;
    _encoder_shape_widget = nullptr;
    _recurrent_binding_widgets.clear();
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

    // ── Encoder shape configuration (GeneralEncoderModel only) ──
    if (_current_info->model_id == "general_encoder") {
        _encoder_shape_widget = new dl::widget::EncoderShapeWidget(
                _state, _assembler.get(), _dynamic_container);
        connect(_encoder_shape_widget,
                &dl::widget::EncoderShapeWidget::shapeApplied,
                this,
                [this]() {
                    _current_info = _assembler->currentModelDisplayInfo();
                    _rebuildSlotPanels();
                    // Now that shape is configured, attempt to load weights
                    // if a path was already entered (gate is now satisfied).
                    _loadModelIfReady();
                });
        _dynamic_layout->addWidget(_encoder_shape_widget);
    }

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
            auto * slot_widget = new dl::widget::DynamicInputSlotWidget(
                    slot, _data_manager, _dynamic_container);
            connect(slot_widget, &dl::widget::DynamicInputSlotWidget::bindingChanged,
                    this, &DeepLearningPropertiesWidget::_syncBindingsFromUi);
            _dynamic_input_widgets.push_back(slot_widget);
            _dynamic_layout->addWidget(slot_widget);
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
            if (slot.hasSequenceDim()) {
                std::vector<std::string> output_names;
                if (_current_info) {
                    for (auto const & out: _current_info->outputs) {
                        output_names.push_back(out.name);
                    }
                }
                auto * seq_widget = new dl::widget::SequenceSlotWidget(
                        slot, _data_manager, output_names, _dynamic_container);
                seq_widget->setEntriesFromState(_state->staticInputs(),
                                                _state->recurrentBindings());
                connect(seq_widget,
                        &dl::widget::SequenceSlotWidget::bindingChanged,
                        this,
                        &DeepLearningPropertiesWidget::_syncBindingsFromUi);
                connect(seq_widget,
                        &dl::widget::SequenceSlotWidget::captureRequested,
                        this,
                        &DeepLearningPropertiesWidget::_onCaptureSequenceEntry);
                connect(seq_widget,
                        &dl::widget::SequenceSlotWidget::captureInvalidated,
                        this,
                        [this](std::string const & slot_name, int memory_index) {
                            auto const key =
                                    staticCacheKey(slot_name, memory_index);
                            _assembler->clearStaticCacheEntry(key);
                            emit staticCacheChanged();
                        });
                _sequence_slot_widgets.push_back(seq_widget);
                _dynamic_layout->addWidget(seq_widget);
            } else {
                // Non-sequence static slot: use the new StaticInputSlotWidget
                auto * slot_widget = new dl::widget::StaticInputSlotWidget(
                        slot, _data_manager, _dynamic_container);
                connect(slot_widget,
                        &dl::widget::StaticInputSlotWidget::bindingChanged,
                        this,
                        &DeepLearningPropertiesWidget::_syncBindingsFromUi);
                connect(slot_widget,
                        &dl::widget::StaticInputSlotWidget::captureRequested,
                        this,
                        &DeepLearningPropertiesWidget::_onCaptureStaticInput);
                connect(slot_widget,
                        &dl::widget::StaticInputSlotWidget::captureInvalidated,
                        this,
                        [this](std::string const & slot_name) {
                            auto const key = staticCacheKey(slot_name, 0);
                            _assembler->clearStaticCacheEntry(key);
                            emit staticCacheChanged();
                        });
                _static_input_widgets.push_back(slot_widget);
                _dynamic_layout->addWidget(slot_widget);
            }
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
            auto * rb_widget = new dl::widget::RecurrentBindingWidget(
                    slot, outputs, _data_manager, _dynamic_container);
            for (auto const & rb: _state->recurrentBindings()) {
                if (rb.input_slot_name == slot.name) {
                    rb_widget->setParams(
                            dl::widget::RecurrentBindingWidget::paramsFromBinding(
                                    rb));
                    break;
                }
            }
            connect(rb_widget, &dl::widget::RecurrentBindingWidget::bindingChanged,
                    this, &DeepLearningPropertiesWidget::_syncBindingsFromUi);
            _recurrent_binding_widgets.push_back(rb_widget);
            _dynamic_layout->addWidget(rb_widget);
        }
    }

    // ── Outputs ──
    if (!outputs.empty()) {
        _dynamic_layout->addWidget(
                new QLabel(tr("<b>Outputs</b>"), _dynamic_container));
    }
    for (auto const & slot: outputs) {
        auto * slot_widget = new dl::widget::OutputSlotWidget(
                slot, _data_manager, _dynamic_container);
        auto const & saved = _state->outputBindings();
        auto it = std::find_if(saved.begin(), saved.end(),
                               [&slot](OutputBindingData const & b) {
                                   return b.slot_name == slot.name;
                               });
        if (it != saved.end()) {
            slot_widget->setParams(
                    dl::widget::OutputSlotWidget::paramsFromBinding(*it));
        }
        connect(slot_widget, &dl::widget::OutputSlotWidget::bindingChanged,
                this, &DeepLearningPropertiesWidget::_syncBindingsFromUi);
        _output_slot_widgets.push_back(slot_widget);
        _dynamic_layout->addWidget(slot_widget);
    }

    // ── Post-Encoder Module ──
    auto * post_encoder_widget = new dl::widget::PostEncoderWidget(
            _state, _data_manager, _assembler.get(), _dynamic_container);
    connect(post_encoder_widget, &dl::widget::PostEncoderWidget::bindingChanged,
            this, &DeepLearningPropertiesWidget::_enforcePostEncoderDecoderConsistency);
    _post_encoder_widget = post_encoder_widget;
    _dynamic_layout->addWidget(post_encoder_widget);

    // Apply initial consistency (honours saved post-encoder state)
    _enforcePostEncoderDecoderConsistency();

    _dynamic_layout->addStretch();
}

// ────────────────────────────────────────────────────────────────────────────
// Post-Encoder / Decoder consistency
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_enforcePostEncoderDecoderConsistency() {
    if (!_post_encoder_widget || !_current_info) return;

    auto const module_type = _post_encoder_widget->moduleTypeForState();
    auto const valid_decoders = dl::constraints::validDecodersForModule(module_type);

    for (auto * slot_widget: _output_slot_widgets) {
        slot_widget->updateDecoderAlternatives(valid_decoders);
        slot_widget->refreshDataSources();
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

    // Refresh dynamic input source combos via DynamicInputSlotWidget
    for (auto * slot_widget: _dynamic_input_widgets) {
        slot_widget->refreshDataSources();
    }

    // Refresh static input source combos
    for (auto const & slot: _current_info->inputs) {
        if (slot.is_static && !slot.is_boolean_mask) {
            if (slot.hasSequenceDim()) {
                for (auto * seq_widget: _sequence_slot_widgets) {
                    if (seq_widget->slotName() == slot.name) {
                        seq_widget->refreshDataSources();
                        break;
                    }
                }
            } else {
                for (auto * slot_widget: _static_input_widgets) {
                    if (slot_widget->slotName() == slot.name) {
                        slot_widget->refreshDataSources();
                        break;
                    }
                }
            }
        }
    }

    // Refresh output target combos
    for (auto * slot_widget: _output_slot_widgets) {
        slot_widget->refreshDataSources();
    }

    // Refresh recurrent binding data_key combos (StaticCapture init)
    for (auto * rb_widget: _recurrent_binding_widgets) {
        rb_widget->refreshDataSources();
    }

    // Refresh post-encoder point_key combo
    if (_post_encoder_widget) {
        _post_encoder_widget->refreshDataSources();
    }
}

// ────────────────────────────────────────────────────────────────────────────
// State Sync
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_syncBindingsFromUi() {
    if (!_current_info) return;

    // ── Input bindings (from DynamicInputSlotWidgets) ──
    std::vector<SlotBindingData> input_bindings;
    for (auto const * w: _dynamic_input_widgets) {
        auto binding =
                dl::conversion::fromDynamicInputParams(w->slotName(), w->params());
        if (!binding.data_key.empty() && binding.data_key != "(None)") {
            input_bindings.push_back(std::move(binding));
        }
    }
    _state->setInputBindings(std::move(input_bindings));

    // ── Output bindings ──
    std::vector<OutputBindingData> output_bindings;
    for (auto const * w: _output_slot_widgets) {
        auto binding =
                dl::conversion::fromOutputParams(w->slotName(), w->params());
        if (!binding.data_key.empty() && binding.data_key != "(None)") {
            output_bindings.push_back(std::move(binding));
        }
    }
    _state->setOutputBindings(std::move(output_bindings));

    // ── Static inputs ──
    std::vector<StaticInputData> static_inputs;
    std::vector<RecurrentBindingData> hybrid_recurrent_bindings;

    for (auto const & slot: _current_info->inputs) {
        if (!slot.is_static || slot.is_boolean_mask) continue;

        if (slot.hasSequenceDim()) {
            for (auto const * seq_widget: _sequence_slot_widgets) {
                if (seq_widget->slotName() != slot.name) continue;
                for (auto si: seq_widget->getStaticInputs()) {
                    if (si.captured_frame < 0) {
                        for (auto const & prev: _state->staticInputs()) {
                            if (prev.slot_name == slot.name &&
                                prev.memory_index == si.memory_index) {
                                si.captured_frame = prev.captured_frame;
                                break;
                            }
                        }
                    }
                    static_inputs.push_back(std::move(si));
                }
                for (auto const & rb: seq_widget->getRecurrentBindings()) {
                    hybrid_recurrent_bindings.push_back(rb);
                }
                break;
            }
        } else {
            for (auto const * w: _static_input_widgets) {
                if (w->slotName() != slot.name) continue;
                auto si = dl::conversion::fromStaticInputParams(
                        w->slotName(), w->params(), w->capturedFrame());
                if (si.captured_frame < 0) {
                    for (auto const & prev: _state->staticInputs()) {
                        if (prev.slot_name == slot.name) {
                            si.captured_frame = prev.captured_frame;
                            break;
                        }
                    }
                }
                if (!si.data_key.empty() && si.data_key != "(None)") {
                    static_inputs.push_back(std::move(si));
                }
                break;
            }
        }
    }
    _state->setStaticInputs(std::move(static_inputs));

    // ── Recurrent bindings ──
    std::vector<RecurrentBindingData> recurrent_bindings =
            std::move(hybrid_recurrent_bindings);
    for (auto const * w: _recurrent_binding_widgets) {
        auto rb = dl::conversion::fromRecurrentParams(w->slotName(), w->params());
        if (rb.output_slot_name.empty()) continue;
        recurrent_bindings.push_back(std::move(rb));
    }
    _state->setRecurrentBindings(std::move(recurrent_bindings));

    _updateBatchSizeConstraint();
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
    _inference_controller->runSingleFrame(_state->currentFrame());
}

void DeepLearningPropertiesWidget::_onRunBatch() {
    if (_inference_controller->isRunning()) {
        _inference_controller->cancel();
        return;
    }

    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    int const current = _state->currentFrame();

    // ── Build dialog ────────────────────────────────────────────────────
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Batch Inference"));
    auto * form = new QFormLayout(&dialog);

    // Mode selection: manual range vs interval series
    auto * manual_radio = new QRadioButton(tr("Manual range"), &dialog);
    auto * interval_radio = new QRadioButton(tr("From interval series"), &dialog);
    manual_radio->setChecked(true);
    form->addRow(manual_radio);

    // Manual range controls
    auto * start_spin = new QSpinBox(&dialog);
    start_spin->setRange(0, 999999);
    start_spin->setValue(current);
    auto * end_spin = new QSpinBox(&dialog);
    end_spin->setRange(0, 999999);
    end_spin->setValue(current + _state->batchSize());
    form->addRow(tr("Start frame:"), start_spin);
    form->addRow(tr("End frame:"), end_spin);

    // Interval series controls
    form->addRow(interval_radio);
    auto * interval_combo = new QComboBox(&dialog);
    interval_combo->setEnabled(false);

    // Populate with current DigitalIntervalSeries keys
    auto const populate_combo = [this, interval_combo]() {
        auto const keys = getKeysForTypes(
                *_data_manager, {DM_DataType::DigitalInterval});
        QString const prev = interval_combo->currentText();
        interval_combo->clear();
        for (auto const & k: keys) {
            interval_combo->addItem(QString::fromStdString(k));
        }
        // Restore previous selection if still available
        int const idx = interval_combo->findText(prev);
        if (idx >= 0) {
            interval_combo->setCurrentIndex(idx);
        }
    };
    populate_combo();

    // Register DataManager observer for live updates, store callback ID
    int dm_cb_id = -1;
    if (_data_manager) {
        dm_cb_id = _data_manager->addObserver(populate_combo);
    }
    // Clean up observer when dialog closes
    QObject::connect(&dialog, &QObject::destroyed,
                     this, [this, dm_cb_id]() {
                         if (_data_manager && dm_cb_id >= 0) {
                             _data_manager->removeObserver(dm_cb_id);
                         }
                     });

    form->addRow(tr("Interval series:"), interval_combo);

    // Toggle enable state based on radio selection
    auto const update_mode = [=](bool manual_checked) {
        start_spin->setEnabled(manual_checked);
        end_spin->setEnabled(manual_checked);
        interval_combo->setEnabled(!manual_checked);
    };
    QObject::connect(manual_radio, &QRadioButton::toggled, &dialog, update_mode);

    // OK / Cancel buttons
    auto * buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // ── Execute dialog ──────────────────────────────────────────────────
    if (dialog.exec() != QDialog::Accepted) return;

    _syncBindingsFromUi();

    if (manual_radio->isChecked()) {
        int const start_frame = start_spin->value();
        int const end_frame = end_spin->value();

        if (end_frame < start_frame) {
            QMessageBox::warning(this, tr("Invalid Range"),
                                 tr("End frame must be >= start frame."));
            return;
        }
        _inference_controller->runBatch(start_frame, end_frame,
                                        _state->batchSize());
    } else {
        // Interval series mode
        auto const key = interval_combo->currentText().toStdString();
        if (key.empty()) {
            QMessageBox::warning(this, tr("No Interval Selected"),
                                 tr("Select an interval series."));
            return;
        }
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (!series || series->size() == 0) {
            QMessageBox::warning(this, tr("Empty Series"),
                                 tr("The selected interval series has no intervals."));
            return;
        }

        std::vector<std::pair<int64_t, int64_t>> intervals;
        intervals.reserve(series->size());
        for (auto const & elem: series->view()) {
            intervals.emplace_back(elem.interval.start, elem.interval.end);
        }

        _inference_controller->runBatchIntervals(
                std::move(intervals), _state->batchSize());
    }
}

void DeepLearningPropertiesWidget::_onInferenceBatchFinished(bool success,
                                                             QString const & error_message) {
    if (success) {
        _weights_status_label->setText(tr("\u2713 Inference complete"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        if (!error_message.isEmpty()) {
            QMessageBox::critical(this, tr("Inference Error"),
                                  tr("Inference failed:\n%1").arg(error_message));
        }
    }
}

void DeepLearningPropertiesWidget::_onInferenceRunningChanged(bool running) {
    if (_run_single_btn) _run_single_btn->setEnabled(!running);
    if (_run_recurrent_btn) _run_recurrent_btn->setEnabled(!running);
    if (_run_batch_btn) {
        _run_batch_btn->setText(running ? tr("Cancel Batch") : tr("Run Batch"));
    }
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

    // Update the StaticInputSlotWidget capture status display
    auto const key = staticCacheKey(slot_name, 0);
    for (auto * slot_widget: _static_input_widgets) {
        if (slot_widget->slotName() == slot_name) {
            if (ok && _assembler->hasStaticCacheEntry(key)) {
                auto const range = _assembler->staticCacheTensorRange(key);
                slot_widget->setCapturedStatus(frame, range);
            } else {
                slot_widget->clearCapturedStatus();
            }
            break;
        }
    }
    emit staticCacheChanged();
}

void DeepLearningPropertiesWidget::_onCaptureSequenceEntry(
        std::string const & slot_name, int memory_index) {

    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    dl::widget::SequenceSlotWidget * seq_widget = nullptr;
    for (auto * w: _sequence_slot_widgets) {
        if (w->slotName() == slot_name) {
            seq_widget = w;
            break;
        }
    }
    if (!seq_widget) return;

    StaticInputData entry;
    bool found = false;
    for (auto const & si: seq_widget->getStaticInputs()) {
        if (si.memory_index == memory_index) {
            entry = si;
            found = true;
            break;
        }
    }
    if (!found || entry.data_key.empty() || entry.data_key == "(None)") {
        QMessageBox::warning(this, tr("No Source"),
                             tr("Select a data source for this entry first."));
        return;
    }

    int frame = _state->currentFrame();
    if (_current_time_position.has_value()) {
        frame = static_cast<int>(_current_time_position->index.getValue());
    }

    ImageSize source_size{256, 256};
    for (auto const & binding: _state->inputBindings()) {
        auto media = _data_manager->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    bool const ok =
            _assembler->captureStaticInput(*_data_manager, entry, frame,
                                           source_size);

    if (ok) {
        auto const key = staticCacheKey(slot_name, memory_index);
        auto const range = _assembler->hasStaticCacheEntry(key)
                                   ? _assembler->staticCacheTensorRange(key)
                                   : std::pair<float, float>{0.0f, 1.0f};
        seq_widget->setCapturedStatus(memory_index, frame, range);
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
        seq_widget->clearCapturedStatus(memory_index);
    }

    emit staticCacheChanged();
}

// ────────────────────────────────────────────────────────────────────────────
// Batch Size Constraint
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_updateBatchSizeConstraint() {
    if (!_dynamic_container || !_batch_size_spin || !_current_info) return;

    auto const constraint = dl::constraints::computeBatchSizeConstraint(
            *_current_info, _state->recurrentBindings());

    auto const & mode = _current_info->batch_mode;
    bool const lock_to_one = (constraint.min == 1 && constraint.max == 1);
    QString tooltip;

    if (lock_to_one) {
        if (constraint.forced_by_recurrent) {
            tooltip = tr("Batch size is locked to 1 when recurrent bindings are active.\n"
                         "Sequential frame-by-frame processing requires batch=1.");
        } else if (std::holds_alternative<dl::RecurrentOnlyBatch>(mode)) {
            tooltip = tr("Model requires batch size = 1 (recurrent-only mode).");
        } else {
            tooltip = tr("Model is compiled for fixed batch size = 1.");
        }
        _batch_size_spin->setValue(1);
        _batch_size_spin->setEnabled(false);
        _batch_size_spin->setToolTip(tooltip);
    } else if (auto const * f = std::get_if<dl::FixedBatch>(&mode)) {
        // Fixed batch > 1: lock spinbox to that value
        _batch_size_spin->setRange(f->size, f->size);
        _batch_size_spin->setValue(f->size);
        _batch_size_spin->setEnabled(false);
        _batch_size_spin->setToolTip(
                tr("Model is compiled for fixed batch size = %1.")
                        .arg(f->size));
    } else {
        // Dynamic batch: allow the model-reported range
        _batch_size_spin->setEnabled(true);
        _batch_size_spin->setMinimum(constraint.min > 0 ? constraint.min : 1);
        if (constraint.max > 0) {
            _batch_size_spin->setMaximum(constraint.max);
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

    auto const & recurrent = _state->recurrentBindings();
    if (recurrent.empty()) {
        QMessageBox::warning(this, tr("No Recurrent Bindings"),
                             tr("Configure at least one recurrent feedback binding\n"
                                "by selecting an output slot in the Recurrent Inputs section."));
        return;
    }

    int const start_frame = _state->currentFrame();
    int const default_count =
            _state->batchSize() > 1 ? _state->batchSize()
                                    : _batch_size_spin->maximum();

    auto * frame_count_spin = new QSpinBox(this);
    frame_count_spin->setRange(1, 999999);
    frame_count_spin->setValue(std::min(100, default_count));

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

    _syncBindingsFromUi();
    _inference_controller->runRecurrentSequence(start_frame,
                                                frame_count_spin->value());
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
    int const frame =
            static_cast<int>(_current_time_position->index.getValue());
    _inference_controller->runSingleFrame(frame);
}
