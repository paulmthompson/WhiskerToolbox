#ifndef MASKHOLEFILLING_WIDGET_HPP
#define MASKHOLEFILLING_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui { class MaskHoleFilling_Widget; }

class MaskHoleFilling_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskHoleFilling_Widget(QWidget *parent = nullptr);
    ~MaskHoleFilling_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::MaskHoleFilling_Widget *ui;
};

#endif // MASKHOLEFILLING_WIDGET_HPP 