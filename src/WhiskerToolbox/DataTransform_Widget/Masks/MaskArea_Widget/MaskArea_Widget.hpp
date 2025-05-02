#ifndef MASKAREA_WIDGET_HPP
#define MASKAREA_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/data_transforms.hpp"

namespace Ui { class MaskArea_Widget; }

class MaskArea_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskArea_Widget(QWidget *parent = nullptr);
    ~MaskArea_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::MaskArea_Widget *ui;
};


#endif// MASKAREA_WIDGET_HPP
