#ifndef ANALOGINTERVALTHRESHOLD_WIDGET_HPP
#define ANALOGINTERVALTHRESHOLD_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/data_transforms.hpp"

namespace Ui { class AnalogIntervalThreshold_Widget; }

class AnalogIntervalThreshold_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit AnalogIntervalThreshold_Widget(QWidget *parent = nullptr);
    ~AnalogIntervalThreshold_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::AnalogIntervalThreshold_Widget *ui;
};


#endif// ANALOGINTERVALTHRESHOLD_WIDGET_HPP
