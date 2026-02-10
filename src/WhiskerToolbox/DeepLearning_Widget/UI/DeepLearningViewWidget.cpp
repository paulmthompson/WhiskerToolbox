#include "DeepLearningViewWidget.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include "DataManager/DataManager.hpp"

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

    // Preview area (for tensor channel thumbnails — future)
    _preview_area = new QWidget(this);
    _preview_layout = new QVBoxLayout(_preview_area);
    _preview_layout->setContentsMargins(0, 0, 0, 0);

    auto * placeholder = new QLabel(
        tr("Tensor channel previews will appear here\n"
           "after running inference."), _preview_area);
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
