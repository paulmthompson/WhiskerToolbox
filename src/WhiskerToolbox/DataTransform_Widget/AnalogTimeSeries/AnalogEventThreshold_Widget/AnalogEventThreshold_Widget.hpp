#ifndef NEURALYZER_V2_ANALOGEVENTTHRESHOLD_WIDGET_HPP
#define NEURALYZER_V2_ANALOGEVENTTHRESHOLD_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui { class AnalogEventThreshold_Widget; }

class AnalogEventThreshold_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit AnalogEventThreshold_Widget(QWidget *parent = nullptr);
    ~AnalogEventThreshold_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::AnalogEventThreshold_Widget *ui;
};


#endif//NEURALYZER_V2_ANALOGEVENTTHRESHOLD_WIDGET_HPP
