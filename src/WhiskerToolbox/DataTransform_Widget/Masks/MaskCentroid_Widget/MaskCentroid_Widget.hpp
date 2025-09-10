#ifndef MASKCENTROID_WIDGET_HPP
#define MASKCENTROID_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui {
class MaskCentroid_Widget;
}

class MaskCentroid_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskCentroid_Widget(QWidget * parent = nullptr);
    ~MaskCentroid_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::MaskCentroid_Widget * ui;
};

#endif// MASKCENTROID_WIDGET_HPP