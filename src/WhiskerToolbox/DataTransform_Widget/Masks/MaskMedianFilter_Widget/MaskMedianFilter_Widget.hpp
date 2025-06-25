#ifndef MASKMEDIANFILTER_WIDGET_HPP
#define MASKMEDIANFILTER_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Masks/mask_median_filter.hpp"

namespace Ui { class MaskMedianFilter_Widget; }

class MaskMedianFilter_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskMedianFilter_Widget(QWidget *parent = nullptr);
    ~MaskMedianFilter_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onWindowSizeChanged(int value);

private:
    Ui::MaskMedianFilter_Widget *ui;
};

#endif // MASKMEDIANFILTER_WIDGET_HPP 