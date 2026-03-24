#ifndef MASKCLEANING_WIDGET_HPP
#define MASKCLEANING_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui {
class MaskCleaning_Widget;
}

class MaskCleaning_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskCleaning_Widget(QWidget * parent = nullptr);
    ~MaskCleaning_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::MaskCleaning_Widget * ui;
};

#endif // MASKCLEANING_WIDGET_HPP
