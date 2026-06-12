/**
 * @file DataBankPropertiesWidget.cpp
 * @brief Implementation of the memory bank properties panel section.
 */

#include "DataBankPropertiesWidget.hpp"

#include "ui_DataBankPropertiesWidget.h"

namespace dl::widget {

DataBankPropertiesWidget::DataBankPropertiesWidget(QWidget * parent)
    : QWidget(parent),
      _ui(std::make_unique<Ui::DataBankPropertiesWidget>()) {
    _ui->setupUi(this);
}

DataBankPropertiesWidget::~DataBankPropertiesWidget() = default;

}// namespace dl::widget
