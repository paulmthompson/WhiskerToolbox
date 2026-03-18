#include "DeepLearningPropertiesWidget.hpp"

#include "DeepLearning_Widget/Core/BatchInferenceResult.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/Core/WriteReservation.hpp"
#include "DeepLearning_Widget/UI/Helpers/DynamicInputSlotWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/OutputSlotWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/PostEncoderWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/RecurrentBindingWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/SequenceSlotWidget.hpp"
#include "DeepLearning_Widget/UI/Helpers/StaticInputSlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"

#include <QComboBox>
#include <QCoreApplication>
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
#include <QStandardItemModel>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <variant>

// ════════════════════════════════════════════════════════════════════════════
// BatchInferenceWorker — runs SlotAssembler::runBatchRangeOffline on a
// background QThread.  Follows the PipelineWorker pattern from MLCoreWidget.
// ════════════════════════════════════════════════════════════════════════════

namespace {

class BatchInferenceWorker : public QThread {
    Q_OBJECT

public:
    BatchInferenceWorker(
            SlotAssembler * assembler,
            DataManager * dm,
            SlotAssembler::MediaOverrides media_overrides,
            std::vector<SlotBindingData> input_bindings,
            std::vector<StaticInputData> static_inputs,
            std::vector<OutputBindingData> output_bindings,
            int start_frame,
            int end_frame,
            ImageSize source_image_size,
            std::shared_ptr<WriteReservation> reservation,
            QObject * parent = nullptr)
        : QThread(parent),
          _assembler(assembler),
          _dm(dm),
          _media_overrides(std::move(media_overrides)),
          _input_bindings(std::move(input_bindings)),
          _static_inputs(std::move(static_inputs)),
          _output_bindings(std::move(output_bindings)),
          _start_frame(start_frame),
          _end_frame(end_frame),
          _source_image_size(source_image_size),
          _reservation(std::move(reservation)) {}

    /// Request cancellation from the main thread.
    void requestCancel() { _cancel_requested.store(true, std::memory_order_relaxed); }

    /// Whether an error occurred during inference.
    [[nodiscard]] bool success() const { return _success; }

    /// Error message (non-empty when success() is false).
    [[nodiscard]] std::string const & errorMessage() const { return _error_message; }

signals:
    /// Emitted from the worker thread; Qt queued connection delivers on main.
    void progressChanged(int _t1, int _t2);

protected:
    void run() override {
        auto result = _assembler->runBatchRangeOffline(
                *_dm,
                _media_overrides,
                _input_bindings,
                _static_inputs,
                _output_bindings,
                _start_frame,
                _end_frame,
                _source_image_size,
                _cancel_requested,
                [this](int current, int total) {
                    emit progressChanged(current, total);
                },
                [this](std::vector<FrameResult> frame_results) {
                    _reservation->push(std::move(frame_results));
                });
        _success = result.success;
        _error_message = std::move(result.error_message);
    }

private:
    SlotAssembler * _assembler;
    DataManager * _dm;
    SlotAssembler::MediaOverrides _media_overrides;
    std::vector<SlotBindingData> _input_bindings;
    std::vector<StaticInputData> _static_inputs;
    std::vector<OutputBindingData> _output_bindings;
    int _start_frame;
    int _end_frame;
    ImageSize _source_image_size;
    std::shared_ptr<WriteReservation> _reservation;
    std::atomic<bool> _cancel_requested{false};
    bool _success = true;
    std::string _error_message;
};

}// namespace

// Pull in the MOC for the anonymous-namespace Q_OBJECT.
// The MOC file is generated from this .cpp, not the header.
#include "DeepLearningPropertiesWidget.moc"

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

    connect(_state.get(), &DeepLearningState::postEncoderModuleChanged, this,
            [this] {
                if (_post_encoder_widget) {
                    _post_encoder_widget->refreshDataSources();
                }
            });
}

DeepLearningPropertiesWidget::~DeepLearningPropertiesWidget() {
    // Stop the merge timer
    if (_merge_timer) {
        _merge_timer->stop();
        delete _merge_timer;
        _merge_timer = nullptr;
    }

    // Stop any running batch inference worker
    if (_batch_worker) {
        dynamic_cast<BatchInferenceWorker *>(_batch_worker)->requestCancel();
        _batch_worker->wait();
        delete _batch_worker;
        _batch_worker = nullptr;
    }

    _write_reservation.reset();

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

        // Apply saved encoder shape if configured
        if (_current_info && model_id == "general_encoder") {
            _applyEncoderShape();
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
    _dynamic_input_widgets.clear();
    _static_input_widgets.clear();
    _sequence_slot_widgets.clear();
    _output_slot_widgets.clear();
    _post_encoder_widget = nullptr;
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
        _buildEncoderShapeSection();
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

    // Both global_avg_pool and spatial_point collapse spatial dims:
    //   [B, C, H, W] -> [B, C]
    // Only TensorToFeatureVector can decode a 2D tensor.
    bool const spatial_dims_removed =
            (module_type == "global_avg_pool" || module_type == "spatial_point");

    std::vector<std::string> const all_decoders = {
            "MaskDecoderParams", "PointDecoderParams", "LineDecoderParams",
            "FeatureVectorDecoderParams"};
    std::vector<std::string> const fv_only = {"FeatureVectorDecoderParams"};

    for (auto * slot_widget: _output_slot_widgets) {
        slot_widget->updateDecoderAlternatives(
                spatial_dims_removed ? fv_only : all_decoders);
        slot_widget->refreshDataSources();
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Encoder Shape Configuration Section
// ────────────────────────────────────────────────────────────────────────────

void DeepLearningPropertiesWidget::_buildEncoderShapeSection() {
    auto * group = new QGroupBox(tr("Encoder Shape"), _dynamic_container);
    auto * form = new QFormLayout(group);
    group->setToolTip(
            tr("Configure the input resolution and output shape to match your\n"
               "compiled model. The defaults (224\u00D7224 input, 384\u00D77\u00D77 output)\n"
               "only apply to the default ConvNeXt-Tiny backbone.\n\n"
               "Set these BEFORE loading weights."));

    // ── Input Height ──
    auto * height_spin = new QSpinBox(group);
    height_spin->setRange(1, 4096);
    height_spin->setSuffix(QStringLiteral(" px"));
    int const saved_h = _state->encoderInputHeight();
    height_spin->setValue(saved_h > 0 ? saved_h : 224);
    form->addRow(tr("Input Height:"), height_spin);

    // ── Input Width ──
    auto * width_spin = new QSpinBox(group);
    width_spin->setRange(1, 4096);
    width_spin->setSuffix(QStringLiteral(" px"));
    int const saved_w = _state->encoderInputWidth();
    width_spin->setValue(saved_w > 0 ? saved_w : 224);
    form->addRow(tr("Input Width:"), width_spin);

    // ── Output Shape ──
    auto * shape_edit = new QLineEdit(group);
    shape_edit->setPlaceholderText(QStringLiteral("384,7,7"));
    shape_edit->setToolTip(
            tr("Comma-separated output dimensions (excluding batch), e.g.:\n"
               "  384,7,7   — spatial feature map\n"
               "  768,16,16 — larger backbone\n"
               "  512       — 1D feature vector"));
    auto const & saved_shape = _state->encoderOutputShape();
    if (!saved_shape.empty()) {
        shape_edit->setText(QString::fromStdString(saved_shape));
    }
    form->addRow(tr("Output Shape:"), shape_edit);

    // ── Apply button ──
    auto * apply_btn = new QPushButton(tr("Apply Shape"), group);
    apply_btn->setToolTip(
            tr("Apply the configured shape to the model.\n"
               "You must re-load weights after changing the shape."));
    form->addRow(apply_btn);

    // Connections
    connect(height_spin, &QSpinBox::valueChanged, this,
            [this](int val) { _state->setEncoderInputHeight(val); });
    connect(width_spin, &QSpinBox::valueChanged, this,
            [this](int val) { _state->setEncoderInputWidth(val); });
    connect(shape_edit, &QLineEdit::textChanged, this,
            [this](QString const & text) {
                _state->setEncoderOutputShape(text.toStdString());
            });
    connect(apply_btn, &QPushButton::clicked, this, [this] {
        _applyEncoderShape();
        _rebuildSlotPanels();
    });

    _dynamic_layout->addWidget(group);
}

void DeepLearningPropertiesWidget::_applyEncoderShape() {
    int const h = _state->encoderInputHeight();
    int const w = _state->encoderInputWidth();

    // Parse output shape string
    std::vector<int64_t> out_shape;
    auto const & shape_str = _state->encoderOutputShape();
    if (!shape_str.empty()) {
        std::istringstream iss(shape_str);
        std::string token;
        while (std::getline(iss, token, ',')) {
            try {
                auto val = std::stoll(token);
                if (val > 0) {
                    out_shape.push_back(val);
                }
            } catch (std::exception const &) {
                // Non-numeric token in comma-separated shape string — skip it
            }
        }
    }

    int const effective_h = (h > 0) ? h : 224;
    int const effective_w = (w > 0) ? w : 224;

    _assembler->configureModelShape(effective_h, effective_w, out_shape);

    // Refresh the display info from the live model to show updated shapes
    _current_info = _assembler->currentModelDisplayInfo();
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
    for (auto const * slot_widget: _dynamic_input_widgets) {
        auto binding = slot_widget->toSlotBindingData();
        if (!binding.data_key.empty() && binding.data_key != "(None)") {
            input_bindings.push_back(std::move(binding));
        }
    }
    _state->setInputBindings(std::move(input_bindings));

    // ── Output bindings ──
    std::vector<OutputBindingData> output_bindings;
    for (auto const * slot_widget: _output_slot_widgets) {
        auto binding = slot_widget->toOutputBindingData();
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
            // Non-sequence slot: delegate to the StaticInputSlotWidget
            for (auto const * slot_widget: _static_input_widgets) {
                if (slot_widget->slotName() == slot.name) {
                    auto si = slot_widget->toStaticInputData();
                    // Preserve captured_frame from existing state if widget
                    // value is -1 (panel was rebuilt without a recapture)
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
    }
    _state->setStaticInputs(std::move(static_inputs));

    // ── Recurrent bindings ──
    // Start with hybrid recurrent bindings collected from sequence entries
    std::vector<RecurrentBindingData> recurrent_bindings =
            std::move(hybrid_recurrent_bindings);

    // Add whole-slot recurrent bindings from RecurrentBindingWidgets
    for (auto const * rb_widget: _recurrent_binding_widgets) {
        auto rb = rb_widget->toRecurrentBindingData();
        if (rb.output_slot_name.empty()) continue;
        recurrent_bindings.push_back(std::move(rb));
    }
    _state->setRecurrentBindings(std::move(recurrent_bindings));

    // Post-encoder state is managed by PostEncoderWidget (applies on parametersChanged).

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
    if (_batch_worker) {
        // Already running — treat as cancel
        _onCancelBatch();
        return;
    }

    if (!_assembler->isModelReady()) {
        QMessageBox::warning(this, tr("Not Ready"),
                             tr("Model weights not loaded."));
        return;
    }

    _syncBindingsFromUi();

    int const current = _state->currentFrame();

    // Ask user for frame range
    auto * start_spin = new QSpinBox(this);
    start_spin->setRange(0, 999999);
    start_spin->setValue(current);

    auto * end_spin = new QSpinBox(this);
    end_spin->setRange(0, 999999);
    end_spin->setValue(current + _state->batchSize());

    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("Batch Inference"));
    dialog.setText(tr("Run inference independently on each frame in the range."));
    dialog.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

    auto * layout = qobject_cast<QGridLayout *>(dialog.layout());
    if (layout) {
        layout->addWidget(new QLabel(tr("Start frame:"), &dialog), 2, 0);
        layout->addWidget(start_spin, 2, 1);
        layout->addWidget(new QLabel(tr("End frame:"), &dialog), 3, 0);
        layout->addWidget(end_spin, 3, 1);
    }

    if (dialog.exec() != QMessageBox::Ok) return;

    int const start_frame = start_spin->value();
    int const end_frame = end_spin->value();

    if (end_frame < start_frame) {
        QMessageBox::warning(this, tr("Invalid Range"),
                             tr("End frame must be >= start frame."));
        return;
    }

    // Clear any pending feature rows from a previous batch
    _pending_feature_rows.clear();

    // Determine source image size
    ImageSize source_size{256, 256};
    for (auto const & binding: _state->inputBindings()) {
        auto media = _data_manager->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    // Clone MediaData for the worker thread — each VideoData needs its own
    // FFmpeg decoder to avoid seek contention with the UI.
    SlotAssembler::MediaOverrides media_overrides;
    for (auto const & binding: _state->inputBindings()) {
        if (binding.encoder_id != "ImageEncoder" || binding.data_key.empty())
            continue;
        auto media = _data_manager->getData<MediaData>(binding.data_key);
        if (!media) continue;
        if (media->getMediaType() == MediaData::MediaType::Video) {
            auto clone = std::make_shared<VideoData>();
            clone->LoadMedia(media->getFilename());
            media_overrides[binding.data_key] = std::move(clone);
        } else {
            // Non-video media can be shared (images are stateless reads)
            media_overrides[binding.data_key] = media;
        }
    }

    auto reservation = std::make_shared<WriteReservation>();

    auto * worker = new BatchInferenceWorker(
            _assembler.get(),
            _data_manager.get(),
            std::move(media_overrides),
            _state->inputBindings(),
            _state->staticInputs(),
            _state->outputBindings(),
            start_frame,
            end_frame,
            source_size,
            reservation,
            this);

    connect(worker, &BatchInferenceWorker::progressChanged,
            this, &DeepLearningPropertiesWidget::batchProgressChanged);

    connect(worker, &QThread::finished,
            this, &DeepLearningPropertiesWidget::_onBatchFinished);

    _write_reservation = std::move(reservation);
    _batch_worker = worker;
    _setBatchRunning(true);

    // Start a merge timer for progressive visibility (200ms interval)
    _merge_timer = new QTimer(this);
    connect(_merge_timer, &QTimer::timeout,
            this, &DeepLearningPropertiesWidget::_mergeResults);
    _merge_timer->start(200);

    worker->start();
}

void DeepLearningPropertiesWidget::_setBatchRunning(bool running) {
    if (_run_single_btn) _run_single_btn->setEnabled(!running);
    if (_run_recurrent_btn) _run_recurrent_btn->setEnabled(!running);
    if (_run_batch_btn) {
        _run_batch_btn->setText(running ? tr("Cancel Batch") : tr("Run Batch"));
    }
}

void DeepLearningPropertiesWidget::_onCancelBatch() {
    if (!_batch_worker) return;
    dynamic_cast<BatchInferenceWorker *>(_batch_worker)->requestCancel();
}

void DeepLearningPropertiesWidget::_onBatchFinished() {
    // Stop the merge timer
    if (_merge_timer) {
        _merge_timer->stop();
        _merge_timer->deleteLater();
        _merge_timer = nullptr;
    }

    // Final merge to pick up any results since the last timer tick
    _mergeResults();

    // ── Flush accumulated feature vectors to TensorData ──
    // TensorData has no per-frame append; build the whole matrix at once.
    for (auto & [key, rows]: _pending_feature_rows) {
        if (rows.empty()) continue;

        // Sort by frame index so rows are ordered chronologically
        std::sort(rows.begin(), rows.end(),
                  [](auto const & a, auto const & b) {
                      return a.first < b.first;
                  });

        std::size_t const n_rows = rows.size();
        std::size_t const n_cols = rows.front().second.size();

        std::vector<float> flat_data;
        flat_data.reserve(n_rows * n_cols);
        for (auto const & [frame, vec]: rows) {
            flat_data.insert(flat_data.end(), vec.begin(), vec.end());
        }

        auto tensor = std::make_shared<TensorData>(
                TensorData::createOrdinal2D(flat_data, n_rows, n_cols));
        _data_manager->setData<TensorData>(key, tensor, TimeKey("time"));
    }
    _pending_feature_rows.clear();

    auto * worker = dynamic_cast<BatchInferenceWorker *>(_batch_worker);

    // Update UI status
    if (!worker->success()) {
        QMessageBox::critical(
                this, tr("Inference Error"),
                tr("Batch inference failed:\n%1")
                        .arg(QString::fromStdString(worker->errorMessage())));
    } else {
        _weights_status_label->setText(
                tr("\u2713 Batch inference complete"));
        _weights_status_label->setStyleSheet(QStringLiteral("color: green;"));
    }

    worker->deleteLater();
    _batch_worker = nullptr;
    _write_reservation.reset();
    _setBatchRunning(false);
}

void DeepLearningPropertiesWidget::_mergeResults() {
    if (!_write_reservation) return;

    auto pending = _write_reservation->drain();
    if (pending.empty()) return;

    // Write drained results to DataManager on the main thread
    for (auto & fr: pending) {
        TimeFrameIndex const frame_idx(fr.frame_index);

        std::visit([&](auto && decoded) {
            using T = std::decay_t<decltype(decoded)>;
            if constexpr (std::is_same_v<T, Mask2D>) {
                auto mask_data = _data_manager->getData<MaskData>(fr.data_key);
                if (!mask_data) {
                    _data_manager->setData<MaskData>(fr.data_key, TimeKey("media"));
                    mask_data = _data_manager->getData<MaskData>(fr.data_key);
                }
                if (mask_data) {
                    mask_data->addAtTime(frame_idx, std::forward<decltype(decoded)>(decoded), NotifyObservers::No);
                }
            } else if constexpr (std::is_same_v<T, Point2D<float>>) {
                auto point_data = _data_manager->getData<PointData>(fr.data_key);
                if (!point_data) {
                    _data_manager->setData<PointData>(fr.data_key, TimeKey("media"));
                    point_data = _data_manager->getData<PointData>(fr.data_key);
                }
                if (point_data) {
                    point_data->addAtTime(frame_idx, std::forward<decltype(decoded)>(decoded), NotifyObservers::No);
                }
            } else if constexpr (std::is_same_v<T, Line2D>) {
                auto line_data = _data_manager->getData<LineData>(fr.data_key);
                if (!line_data) {
                    _data_manager->setData<LineData>(fr.data_key, TimeKey("media"));
                    line_data = _data_manager->getData<LineData>(fr.data_key);
                }
                if (line_data) {
                    line_data->addAtTime(frame_idx, std::forward<decltype(decoded)>(decoded), NotifyObservers::No);
                }
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                // Accumulate feature vectors — TensorData is written in bulk
                // by _onBatchFinished() after all frames are processed.
                _pending_feature_rows[fr.data_key].emplace_back(
                        fr.frame_index, std::forward<decltype(decoded)>(decoded));
            }
        },
                   fr.data);
    }

    // Notify observers once for each affected data key (not TensorData —
    // those are notified in bulk at batch finish)
    std::set<std::string> affected_keys;
    for (auto const & fr: pending) {
        if (!std::holds_alternative<std::vector<float>>(fr.data)) {
            affected_keys.insert(fr.data_key);
        }
    }
    for (auto const & key: affected_keys) {
        if (auto d = _data_manager->getData<MaskData>(key)) d->notifyObservers();
        if (auto d = _data_manager->getData<PointData>(key)) d->notifyObservers();
        if (auto d = _data_manager->getData<LineData>(key)) d->notifyObservers();
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
            for (auto const * seq_widget: _sequence_slot_widgets) {
                if (seq_widget->slotName() == slot.name &&
                    !seq_widget->getRecurrentBindings().empty()) {
                    has_recurrent = true;
                    break;
                }
            }
        } else {
            for (auto const * rb_widget: _recurrent_binding_widgets) {
                if (rb_widget->slotName() == slot.name) {
                    auto rb = rb_widget->toRecurrentBindingData();
                    if (!rb.output_slot_name.empty()) {
                        has_recurrent = true;
                    }
                    break;
                }
            }
        }
        if (has_recurrent) break;
    }

    // ── Step 3: Determine the most restrictive constraint ──
    // Priority: recurrent bindings → model RecurrentOnly → model Fixed → model Dynamic
    bool const lock_to_one = has_recurrent || model_locked;
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
