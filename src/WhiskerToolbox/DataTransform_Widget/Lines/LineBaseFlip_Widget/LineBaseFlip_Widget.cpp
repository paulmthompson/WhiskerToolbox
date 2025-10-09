#include "LineBaseFlip_Widget.hpp"
#include "ui_LineBaseFlip_Widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QMessageBox>

LineBaseFlip_Widget::LineBaseFlip_Widget(QWidget* parent)
    : TransformParameter_Widget(parent)
    , ui(new Ui::LineBaseFlip_Widget)
{
    ui->setupUi(this);

    // Connect signals
    connect(ui->getFromViewerButton, &QPushButton::clicked, this, &LineBaseFlip_Widget::onGetFromMediaViewer);
}

LineBaseFlip_Widget::~LineBaseFlip_Widget()
{
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineBaseFlip_Widget::getParameters() const {
    Point2D<float> reference_point{
        static_cast<float>(ui->xSpinBox->value()),
        static_cast<float>(ui->ySpinBox->value())
    };

    return std::make_unique<LineBaseFlipParameters>(reference_point);
}

void LineBaseFlip_Widget::onGetFromMediaViewer() {
    // For now, show a message box explaining the feature
    // In a full implementation, this would integrate with the media viewer
    // to allow the user to click and place a reference point

    QMessageBox::information(this, "Get from Media Viewer",
        "This feature would allow you to click in the media viewer to place a reference point.\n\n"
        "Implementation note: This would require integration with the media viewer widget "
        "to capture mouse clicks and convert them to image coordinates.\n\n"
        "For now, please enter the coordinates manually in the X and Y fields above.");
}
