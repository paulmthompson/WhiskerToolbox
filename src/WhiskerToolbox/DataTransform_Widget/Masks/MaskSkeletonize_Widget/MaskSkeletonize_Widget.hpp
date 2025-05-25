#ifndef MASKSKELETONIZE_WIDGET_HPP
#define MASKSKELETONIZE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Masks/mask_skeletonize.hpp"

namespace Ui { class MaskSkeletonize_Widget; }

class MaskSkeletonize_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskSkeletonize_Widget(QWidget *parent = nullptr);
    ~MaskSkeletonize_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private:
    Ui::MaskSkeletonize_Widget *ui;
};

#endif // MASKSKELETONIZE_WIDGET_HPP 