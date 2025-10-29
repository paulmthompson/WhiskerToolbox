#ifndef ANALOGINTERVALPEAK_WIDGET_HPP
#define ANALOGINTERVALPEAK_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

#include <memory>

namespace Ui {
class AnalogIntervalPeak_Widget;
}

class AnalogIntervalPeak_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit AnalogIntervalPeak_Widget(QWidget * parent = nullptr);
    ~AnalogIntervalPeak_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private:
    Ui::AnalogIntervalPeak_Widget * ui;

    void _populateIntervalSeriesComboBox();
};

#endif// ANALOGINTERVALPEAK_WIDGET_HPP
