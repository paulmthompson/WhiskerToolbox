#ifndef GROUPINTERVALS_WIDGET_HPP
#define GROUPINTERVALS_WIDGET_HPP

#include "DataManager/transforms/data_transforms.hpp"
#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui {
class GroupIntervals_Widget;
}

class GroupIntervals_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit GroupIntervals_Widget(QWidget * parent = nullptr);
    ~GroupIntervals_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::GroupIntervals_Widget * ui;
};

#endif// GROUPINTERVALS_WIDGET_HPP