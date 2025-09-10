#ifndef MASKPRINCIPALAXIS_WIDGET_HPP
#define MASKPRINCIPALAXIS_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui {
class MaskPrincipalAxis_Widget;
}

class MaskPrincipalAxis_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskPrincipalAxis_Widget(QWidget * parent = nullptr);
    ~MaskPrincipalAxis_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::MaskPrincipalAxis_Widget * ui;
};

#endif// MASKPRINCIPALAXIS_WIDGET_HPP