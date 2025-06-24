#include "MaskNoneSelectionWidget.hpp"
#include "ui_MaskNoneSelectionWidget.h"

namespace mask_widget {

MaskNoneSelectionWidget::MaskNoneSelectionWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MaskNoneSelectionWidget) {
    ui->setupUi(this);
}

MaskNoneSelectionWidget::~MaskNoneSelectionWidget() {
    delete ui;
}

}// namespace mask_widget