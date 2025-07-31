#include "GlobalPropertiesWidget.hpp"
#include "ui_GlobalPropertiesWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QPushButton>

GlobalPropertiesWidget::GlobalPropertiesWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::GlobalPropertiesWidget) {
    ui->setupUi(this);

    // Connect signals
    connect(ui->background_color_button, &QPushButton::clicked,
            this, &GlobalPropertiesWidget::handleBackgroundColorChanged);

    connect(ui->show_grid_checkbox, &QCheckBox::toggled,
            this, &GlobalPropertiesWidget::handleGridVisibilityChanged);

    connect(ui->snap_to_grid_checkbox, &QCheckBox::toggled,
            this, &GlobalPropertiesWidget::handleSnapToGridChanged);
}

GlobalPropertiesWidget::~GlobalPropertiesWidget() {
    delete ui;
}

void GlobalPropertiesWidget::handleBackgroundColorChanged() {
    QColor current_color = ui->background_color_button->palette().color(QPalette::Button);
    QColor new_color = QColorDialog::getColor(current_color, this, "Choose Background Color");

    if (new_color.isValid()) {
        // Update button appearance
        QString style_sheet = QString("background-color: rgb(%1, %2, %3);")
                                      .arg(new_color.red())
                                      .arg(new_color.green())
                                      .arg(new_color.blue());
        ui->background_color_button->setStyleSheet(style_sheet);

        emit globalPropertiesChanged();
    }
}

void GlobalPropertiesWidget::handleGridVisibilityChanged(bool visible) {
    Q_UNUSED(visible)
    emit globalPropertiesChanged();
}

void GlobalPropertiesWidget::handleSnapToGridChanged(bool enabled) {
    Q_UNUSED(enabled)
    emit globalPropertiesChanged();
}