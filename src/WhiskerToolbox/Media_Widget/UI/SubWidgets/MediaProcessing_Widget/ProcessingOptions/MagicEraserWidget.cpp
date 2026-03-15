/**
 * @file MagicEraserWidget.cpp
 * @brief Drawing controls for the magic eraser tool
 */

#include "MagicEraserWidget.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MagicEraserWidget::MagicEraserWidget(QWidget * parent)
    : QWidget(parent) {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    _drawing_mode_button = new QPushButton(tr("Start Drawing"), this);
    _drawing_mode_button->setCheckable(true);
    _drawing_mode_button->setToolTip(tr("Toggle drawing mode to paint areas for erasing"));
    layout->addWidget(_drawing_mode_button);

    _clear_mask_button = new QPushButton(tr("Clear Mask"), this);
    _clear_mask_button->setToolTip(tr("Clear the current magic eraser mask"));
    layout->addWidget(_clear_mask_button);

    auto * instructions = new QLabel(
            tr("Instructions: Enable drawing mode and paint over areas to erase.\n"
               "The erased areas will be replaced with median-filtered content."),
            this);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);

    connect(_drawing_mode_button, &QPushButton::toggled,
            this, &MagicEraserWidget::_onDrawingModeChanged);
    connect(_clear_mask_button, &QPushButton::clicked,
            this, &MagicEraserWidget::_onClearMaskClicked);
}

bool MagicEraserWidget::isDrawingMode() const {
    return _drawing_mode_button->isChecked();
}

void MagicEraserWidget::setDrawingMode(bool enabled) {
    _drawing_mode_button->blockSignals(true);
    _drawing_mode_button->setChecked(enabled);
    _drawing_mode_button->setText(enabled ? tr("Stop Drawing") : tr("Start Drawing"));
    _drawing_mode_button->blockSignals(false);
}

void MagicEraserWidget::_onDrawingModeChanged() {
    bool const drawing = _drawing_mode_button->isChecked();
    _drawing_mode_button->setText(drawing ? tr("Stop Drawing") : tr("Start Drawing"));
    emit drawingModeChanged(drawing);
}

void MagicEraserWidget::_onClearMaskClicked() {
    emit clearMaskRequested();
}
