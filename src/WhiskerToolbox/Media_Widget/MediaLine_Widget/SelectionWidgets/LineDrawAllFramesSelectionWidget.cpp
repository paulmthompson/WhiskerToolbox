#include "LineDrawAllFramesSelectionWidget.hpp"
#include "ui_LineDrawAllFramesSelectionWidget.h"

#include "CoreGeometry/points.hpp"

#include <QPushButton>
#include <QLabel>

namespace line_widget {

LineDrawAllFramesSelectionWidget::LineDrawAllFramesSelectionWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LineDrawAllFramesSelectionWidget) {
    ui->setupUi(this);
    _setupUI();
    _connectSignals();
}

LineDrawAllFramesSelectionWidget::~LineDrawAllFramesSelectionWidget() {
    delete ui;
}

void LineDrawAllFramesSelectionWidget::_setupUI() {
    // Initial UI state
    _updateUI();
}

void LineDrawAllFramesSelectionWidget::_connectSignals() {
    // Connect button signals
    connect(ui->startDrawingButton, &QPushButton::clicked, 
            this, &LineDrawAllFramesSelectionWidget::_onStartDrawingClicked);
    connect(ui->completeDrawingButton, &QPushButton::clicked, 
            this, &LineDrawAllFramesSelectionWidget::_onCompleteDrawingClicked);
    connect(ui->applyToAllFramesButton, &QPushButton::clicked, 
            this, &LineDrawAllFramesSelectionWidget::_onApplyToAllFramesClicked);
    connect(ui->clearLineButton, &QPushButton::clicked, 
            this, &LineDrawAllFramesSelectionWidget::_onClearLineClicked);
}

void LineDrawAllFramesSelectionWidget::_updateUI() {
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
    ui->applyToAllFramesButton->setEnabled(!_is_drawing_active && !_current_line_points.empty());
    ui->clearLineButton->setEnabled(!_current_line_points.empty());
}

void LineDrawAllFramesSelectionWidget::_onStartDrawingClicked() {
    _is_drawing_active = true;
    _current_line_points.clear();
    _updateUI();
    emit lineDrawingStarted();
}

void LineDrawAllFramesSelectionWidget::_onCompleteDrawingClicked() {
    _is_drawing_active = false;
    _updateUI();
    emit lineDrawingCompleted();
}

void LineDrawAllFramesSelectionWidget::_onApplyToAllFramesClicked() {
    emit applyToAllFrames();
}

void LineDrawAllFramesSelectionWidget::_onClearLineClicked() {
    _is_drawing_active = false;
    _current_line_points.clear();
    _updateUI();
    emit linePointsUpdated();
}

bool LineDrawAllFramesSelectionWidget::isDrawingActive() const {
    return _is_drawing_active;
}

std::vector<Point2D<float>> LineDrawAllFramesSelectionWidget::getCurrentLinePoints() const {
    return _current_line_points;
}

void LineDrawAllFramesSelectionWidget::clearLinePoints() {
    _current_line_points.clear();
    _is_drawing_active = false;
    _updateUI();
    emit linePointsUpdated();
}

/**
 * @brief Add a point to the current line
 * @param point The point to add
 */
void LineDrawAllFramesSelectionWidget::addPoint(const Point2D<float>& point) {
    if (_is_drawing_active) {
        _current_line_points.push_back(point);
        _updateUI();
        emit linePointsUpdated();
    }
}

} // namespace line_widget 