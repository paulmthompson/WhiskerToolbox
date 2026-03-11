#include "DeepLearningViewWidget.hpp"

#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include "DataManager/DataManager.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

DeepLearningViewWidget::DeepLearningViewWidget(
        std::shared_ptr<DeepLearningState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {

    _buildUi();

    connect(_state.get(), &DeepLearningState::modelChanged,
            this, &DeepLearningViewWidget::_onModelChanged);

    _onModelChanged();
}

DeepLearningViewWidget::~DeepLearningViewWidget() = default;

void DeepLearningViewWidget::setAssembler(SlotAssembler * assembler) {
    _assembler = assembler;
}

void DeepLearningViewWidget::_buildUi() {
    auto * main = new QVBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(6);

    // Status
    _status_label = new QLabel(tr("No model selected"), this);
    _status_label->setAlignment(Qt::AlignCenter);
    _status_label->setStyleSheet(
            QStringLiteral("font-size: 14px; font-weight: bold;"));
    main->addWidget(_status_label);

    // Model info
    _model_info_label = new QLabel(this);
    _model_info_label->setWordWrap(true);
    _model_info_label->setAlignment(Qt::AlignCenter);
    _model_info_label->setStyleSheet(
            QStringLiteral("color: gray; font-size: 11px;"));
    main->addWidget(_model_info_label);

    // Preview area
    _preview_area = new QWidget(this);
    _preview_layout = new QVBoxLayout(_preview_area);
    _preview_layout->setContentsMargins(0, 0, 0, 0);

    auto * placeholder = new QLabel(
            tr("Captured static tensor previews will appear here\n"
               "after using \"Capture Current Frame\" in Absolute mode."),
            _preview_area);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(
            QStringLiteral("color: #888; border: 1px dashed #ccc; "
                           "padding: 40px; margin: 20px;"));
    _preview_layout->addWidget(placeholder);

    main->addWidget(_preview_area, /*stretch=*/1);

    // Progress bar (hidden by default)
    _progress_bar = new QProgressBar(this);
    _progress_bar->setVisible(false);
    _progress_bar->setTextVisible(true);
    main->addWidget(_progress_bar);
}

void DeepLearningViewWidget::_clearPreviewArea() {
    if (!_preview_layout) return;
    QLayoutItem * child = nullptr;
    while ((child = _preview_layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
}

void DeepLearningViewWidget::refreshCachePreview() {
    _clearPreviewArea();

    if (!_assembler) {
        auto * label = new QLabel(tr("No assembler connected"), _preview_area);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QStringLiteral("color: gray;"));
        _preview_layout->addWidget(label);
        return;
    }

    auto const keys = _assembler->staticCacheKeys();
    if (keys.empty()) {
        auto * placeholder = new QLabel(
                tr("No cached static tensors.\n"
                   "Set a slot to Absolute mode and click\n"
                   "\"Capture Current Frame\" to cache a tensor."),
                _preview_area);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(
                QStringLiteral("color: #888; border: 1px dashed #ccc; "
                               "padding: 30px; margin: 10px;"));
        _preview_layout->addWidget(placeholder);
        return;
    }

    // Show info for each cached tensor
    for (auto const & key: keys) {
        auto * group = new QGroupBox(
                QString::fromStdString(key), _preview_area);
        auto * layout = new QVBoxLayout(group);

        auto const shape = _assembler->staticCacheTensorShape(key);
        auto const range = _assembler->staticCacheTensorRange(key);

        // Shape
        QString shape_text = tr("Shape: ");
        for (std::size_t i = 0; i < shape.size(); ++i) {
            if (i > 0) shape_text += QStringLiteral(" \u00D7 ");
            shape_text += QString::number(shape[i]);
        }
        auto * shape_label = new QLabel(shape_text, group);
        layout->addWidget(shape_label);

        // Value range
        auto * range_label = new QLabel(
                tr("Value range: [%1, %2]")
                        .arg(static_cast<double>(range.first), 0, 'f', 4)
                        .arg(static_cast<double>(range.second), 0, 'f', 4),
                group);
        layout->addWidget(range_label);

        // Find captured frame from state
        for (auto const & si: _state->staticInputs()) {
            auto const si_key = staticCacheKey(si.slot_name, si.memory_index);
            if (si_key == key && si.captured_frame >= 0) {
                auto * frame_label = new QLabel(
                        tr("Captured at frame: %1").arg(si.captured_frame),
                        group);
                layout->addWidget(frame_label);
                break;
            }
        }

        // Status indicator
        auto * status_label = new QLabel(
                tr("\u2713 Cached and ready for inference"), group);
        status_label->setStyleSheet(
                QStringLiteral("color: green; font-weight: bold;"));
        layout->addWidget(status_label);

        _preview_layout->addWidget(group);
    }

    _preview_layout->addStretch();
}

void DeepLearningViewWidget::_onModelChanged() {
    auto const & model_id = _state->selectedModelId();

    if (model_id.empty()) {
        _status_label->setText(tr("No model selected"));
        _model_info_label->clear();
        return;
    }

    auto info = SlotAssembler::getModelDisplayInfo(model_id);
    if (!info) {
        _status_label->setText(
                tr("Unknown model: %1").arg(QString::fromStdString(model_id)));
        _model_info_label->clear();
        return;
    }

    _status_label->setText(QString::fromStdString(info->display_name));

    // Build slot summary
    QString summary;
    summary += tr("Inputs: ");
    for (std::size_t i = 0; i < info->inputs.size(); ++i) {
        if (i > 0) summary += QStringLiteral(", ");
        summary += QString::fromStdString(info->inputs[i].name);
        if (info->inputs[i].is_static) summary += QStringLiteral(" (static)");
    }
    summary += QStringLiteral("\n");
    summary += tr("Outputs: ");
    for (std::size_t i = 0; i < info->outputs.size(); ++i) {
        if (i > 0) summary += QStringLiteral(", ");
        summary += QString::fromStdString(info->outputs[i].name);
    }
    _model_info_label->setText(summary);
}
