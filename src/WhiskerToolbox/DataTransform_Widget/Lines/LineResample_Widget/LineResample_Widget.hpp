#ifndef LINE_RESAMPLE_WIDGET_HPP
#define LINE_RESAMPLE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui {
class LineResample_Widget;
}

class LineResample_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit LineResample_Widget(QWidget * parent = nullptr);
    ~LineResample_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onAlgorithmChanged();

private:
    void updateParameterVisibility();
    void updateDescription();

    Ui::LineResample_Widget * ui;
};

#endif// LINE_RESAMPLE_WIDGET_HPP
