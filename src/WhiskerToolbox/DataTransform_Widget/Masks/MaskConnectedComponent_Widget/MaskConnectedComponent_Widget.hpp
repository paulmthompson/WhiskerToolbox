#ifndef MASKCONNECTEDCOMPONENT_WIDGET_HPP
#define MASKCONNECTEDCOMPONENT_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Masks/mask_connected_component.hpp"

namespace Ui { class MaskConnectedComponent_Widget; }

class MaskConnectedComponent_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskConnectedComponent_Widget(QWidget *parent = nullptr);
    ~MaskConnectedComponent_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::MaskConnectedComponent_Widget *ui;
};

#endif // MASKCONNECTEDCOMPONENT_WIDGET_HPP 