#ifndef LINEANGLE_WIDGET_HPP
#define LINEANGLE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Lines/line_angle.hpp"

namespace Ui { class LineAngle_Widget; }

class LineAngle_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit LineAngle_Widget(QWidget *parent = nullptr);
    ~LineAngle_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::LineAngle_Widget *ui;
};

#endif// LINEANGLE_WIDGET_HPP
