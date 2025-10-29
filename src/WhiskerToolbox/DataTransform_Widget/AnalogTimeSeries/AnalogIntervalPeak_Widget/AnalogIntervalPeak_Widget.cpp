#include "AnalogIntervalPeak_Widget.hpp"

#include "ui_AnalogIntervalPeak_Widget.h"

#include "DataManager/transforms/AnalogTimeSeries/Analog_Interval_Peak/analog_interval_peak.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <iostream>

AnalogIntervalPeak_Widget::AnalogIntervalPeak_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
      ui(new Ui::AnalogIntervalPeak_Widget) {
    ui->setupUi(this);
}

AnalogIntervalPeak_Widget::~AnalogIntervalPeak_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> AnalogIntervalPeak_Widget::getParameters() const {
    auto params = std::make_unique<IntervalPeakParams>();

    // Get peak type
    QString peak_type_str = ui->peak_type_combobox->currentText();
    if (peak_type_str == "Maximum") {
        params->peak_type = IntervalPeakParams::PeakType::MAXIMUM;
    } else if (peak_type_str == "Minimum") {
        params->peak_type = IntervalPeakParams::PeakType::MINIMUM;
    }

    // Get search mode
    QString search_mode_str = ui->search_mode_combobox->currentText();
    if (search_mode_str == "Within Intervals") {
        params->search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
    } else if (search_mode_str == "Between Interval Starts") {
        params->search_mode = IntervalPeakParams::SearchMode::BETWEEN_INTERVAL_STARTS;
    }

    // Get interval series
    QString interval_series_name = ui->interval_series_combobox->currentText();
    if (!interval_series_name.isEmpty() && dataManager()) {
        auto data_variant = dataManager()->getDataVariant(interval_series_name.toStdString());
        if (data_variant.has_value()) {
            auto const * interval_ptr = std::get_if<std::shared_ptr<DigitalIntervalSeries>>(&data_variant.value());
            if (interval_ptr && *interval_ptr) {
                params->interval_series = *interval_ptr;
            }
        }
    }

    return params;
}

void AnalogIntervalPeak_Widget::onDataManagerChanged() {
    _populateIntervalSeriesComboBox();
}

void AnalogIntervalPeak_Widget::onDataManagerDataChanged() {
    _populateIntervalSeriesComboBox();
}

void AnalogIntervalPeak_Widget::_populateIntervalSeriesComboBox() {
    ui->interval_series_combobox->clear();

    if (!dataManager()) {
        return;
    }

    // Get all DigitalIntervalSeries keys from the data manager
    auto const interval_keys = dataManager()->getKeys<DigitalIntervalSeries>();
    
    for (auto const & key : interval_keys) {
        ui->interval_series_combobox->addItem(QString::fromStdString(key));
    }
}
