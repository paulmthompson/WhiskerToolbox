/**
 * @file MediaDebugPanel.cpp
 * @brief Implementation of the developer diagnostics panel for Media Widget
 */

#include "MediaDebugPanel.hpp"

#include "Core/MediaWidgetState.hpp"
#include "Rendering/Media_Window/Media_Window.hpp"

#include "Common/Collapsible_Widget/Section.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

QLabel * createCountValueLabel(QWidget * parent) {
    auto * label = new QLabel(QStringLiteral("0"), parent);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}

}// namespace

MediaDebugPanel::MediaDebugPanel(std::shared_ptr<MediaWidgetState> state,
                                 Media_Window * media_window,
                                 QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _media_window(media_window) {
    _buildUI();
    _connectSignals();
    refresh();
}

void MediaDebugPanel::setMediaWindow(Media_Window * media_window) {
    if (_media_window == media_window) {
        return;
    }

    if (_media_window != nullptr) {
        disconnect(_media_window, nullptr, this, nullptr);
    }

    _media_window = media_window;
    _connectSignals();
    refresh();
}

void MediaDebugPanel::refresh() {
    _refreshSceneElements();
}

void MediaDebugPanel::_buildUI() {
    auto * root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);

    _scene_elements_section = new Section(this, QStringLiteral("Scene Elements"));
    auto * form_layout = new QFormLayout();
    form_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    _line_paths_label = createCountValueLabel(this);
    _masks_label = createCountValueLabel(this);
    _mask_bounding_boxes_label = createCountValueLabel(this);
    _mask_outlines_label = createCountValueLabel(this);
    _points_label = createCountValueLabel(this);
    _intervals_label = createCountValueLabel(this);
    _tensors_label = createCountValueLabel(this);
    _text_items_label = createCountValueLabel(this);
    _total_scene_items_label = createCountValueLabel(this);

    form_layout->addRow(QStringLiteral("Line Paths:"), _line_paths_label);
    form_layout->addRow(QStringLiteral("Masks:"), _masks_label);
    form_layout->addRow(QStringLiteral("Mask Bounding Boxes:"), _mask_bounding_boxes_label);
    form_layout->addRow(QStringLiteral("Mask Outlines:"), _mask_outlines_label);
    form_layout->addRow(QStringLiteral("Points:"), _points_label);
    form_layout->addRow(QStringLiteral("Intervals:"), _intervals_label);
    form_layout->addRow(QStringLiteral("Tensors:"), _tensors_label);
    form_layout->addRow(QStringLiteral("Text Items:"), _text_items_label);
    form_layout->addRow(QStringLiteral("Total Scene Items:"), _total_scene_items_label);

    _scene_elements_section->setContentLayout(*form_layout);

    root_layout->addWidget(_scene_elements_section);

    // Expand by default so counts are visible when developer mode is enabled.
    _scene_elements_section->toggle(true);
}

void MediaDebugPanel::_connectSignals() {
    if (_media_window == nullptr) {
        return;
    }

    connect(_media_window, &Media_Window::canvasUpdated, this, &MediaDebugPanel::refresh);
}

void MediaDebugPanel::_refreshSceneElements() {
    if (_media_window == nullptr) {
        _line_paths_label->setText(QStringLiteral("0"));
        _masks_label->setText(QStringLiteral("0"));
        _mask_bounding_boxes_label->setText(QStringLiteral("0"));
        _mask_outlines_label->setText(QStringLiteral("0"));
        _points_label->setText(QStringLiteral("0"));
        _intervals_label->setText(QStringLiteral("0"));
        _tensors_label->setText(QStringLiteral("0"));
        _text_items_label->setText(QStringLiteral("0"));
        _total_scene_items_label->setText(QStringLiteral("0"));
        return;
    }

    auto const diagnostics = _media_window->getSceneDiagnostics();

    _line_paths_label->setText(QString::number(diagnostics.line_paths));
    _masks_label->setText(QString::number(diagnostics.masks));
    _mask_bounding_boxes_label->setText(QString::number(diagnostics.mask_bounding_boxes));
    _mask_outlines_label->setText(QString::number(diagnostics.mask_outlines));
    _points_label->setText(QString::number(diagnostics.points));
    _intervals_label->setText(QString::number(diagnostics.intervals));
    _tensors_label->setText(QString::number(diagnostics.tensors));
    _text_items_label->setText(QString::number(diagnostics.text_items));
    _total_scene_items_label->setText(QString::number(diagnostics.total_scene_items));
}
