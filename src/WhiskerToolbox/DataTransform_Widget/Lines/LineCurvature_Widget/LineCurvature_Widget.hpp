#ifndef LINE_CURVATURE_WIDGET_HPP
#define LINE_CURVATURE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Lines/line_curvature.hpp"

namespace Ui {
    class LineCurvature_Widget;
}

class LineCurvature_Widget : public TransformParameter_Widget {
    Q_OBJECT

public:
    explicit LineCurvature_Widget(QWidget *parent = nullptr);
    ~LineCurvature_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onMethodChanged(int index);

private:
    Ui::LineCurvature_Widget *ui;
};

#endif // LINE_CURVATURE_WIDGET_HPP 