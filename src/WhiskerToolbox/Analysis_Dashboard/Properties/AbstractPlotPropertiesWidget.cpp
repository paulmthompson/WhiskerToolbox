#include "AbstractPlotPropertiesWidget.hpp"

AbstractPlotPropertiesWidget::AbstractPlotPropertiesWidget(QWidget * parent)
    : QWidget(parent),
      _plot_widget(nullptr),
      _data_manager(nullptr) {
}