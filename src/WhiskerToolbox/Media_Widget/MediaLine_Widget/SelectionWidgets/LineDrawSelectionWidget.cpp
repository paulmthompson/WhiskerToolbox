#include "LineDrawSelectionWidget.hpp"
#include "ui_LineDrawSelectionWidget.h"

#include "CoreGeometry/points.hpp"

#include <QPushButton>
#include <QLabel>

namespace line_widget {

LineDrawSelectionWidget::LineDrawSelectionWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LineDrawSelectionWidget) {
    ui->setupUi(this);
    _setupUI();
    _connectSignals();
}

LineDrawSelectionWidget::~LineDrawSelectionWidget() {
    delete ui;
}

void LineDrawSelectionWidget::_setupUI() {
    // Initial UI state
    _updateUI();
}

void LineDrawSelectionWidget::_connectSignals() {
    // Connect button signals
    connect(ui->startDrawingButton, &QPushButton::clicked,
            this, &LineDrawSelectionWidget::_onStartDrawingClicked);
    connect(ui->completeDrawingButton, &QPushButton::clicked,
            this, &LineDrawSelectionWidget::_onCompleteDrawingClicked);
    connect(ui->applyToCurrentFrameButton, &QPushButton::clicked,
            this, &LineDrawSelectionWidget::_onApplyToCurrentFrameClicked);
    connect(ui->clearLineButton, &QPushButton::clicked,
            this, &LineDrawSelectionWidget::_onClearLineClicked);
}

void LineDrawSelectionWidget::_updateUI() {
    // Update status label
    if (_is_drawing_active) {
        ui->statusLabel->setText("Status: Drawing active - click in video to add points");
    } else {
        ui->statusLabel->setText("Status: Ready to draw");
    }

    // Update points count
    ui->pointsLabel->setText(QString("Points: %1").arg(_current_line_points.size()));

    // Update button states
    ui->startDrawingButton->setEnabled(!_is_drawing_active);
    ui->completeDrawingButton->setEnabled(_is_drawing_active && !_current_line_points.empty());
    ui->applyToCurrentFrameButton->setEnabled(!_is_drawing_active && !_current_line_points.empty());
    ui->clearLineButton->setEnabled(!_current_line_points.empty());
}

void LineDrawSelectionWidget::_onStartDrawingClicked() {
    _is_drawing_active = true;
    _current_line_points.clear();
    _updateUI();
    emit lineDrawingStarted();
}

void LineDrawSelectionWidget::_onCompleteDrawingClicked() {
    _is_drawing_active = false;
    _updateUI();
    emit lineDrawingCompleted();
}

void LineDrawSelectionWidget::_onApplyToCurrentFrameClicked() {
    emit applyToCurrentFrame();
}

void LineDrawSelectionWidget::_onClearLineClicked() {
    _is_drawing_active = false;
    _current_line_points.clear();
    _updateUI();
    emit linePointsUpdated();
}

bool LineDrawSelectionWidget::isDrawingActive() const {
    return _is_drawing_active;
}

std::vector<Point2D<float>> LineDrawSelectionWidget::getCurrentLinePoints() const {
    return _current_line_points;
}

void LineDrawSelectionWidget::clearLinePoints() {
    _current_line_points.clear();
    _is_drawing_active = false;
    _updateUI();
    emit linePointsUpdated();
}

void LineDrawSelectionWidget::addPoint(const Point2D<float>& point) {
    if (_is_drawing_active) {
        _current_line_points.push_back(point);
        _updateUI();
        emit linePointsUpdated();
    }
}

} // namespace line_widget
