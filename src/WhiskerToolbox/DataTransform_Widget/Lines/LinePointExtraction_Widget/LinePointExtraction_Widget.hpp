#ifndef LINEPOINTEXTRACTION_WIDGET_HPP
#define LINEPOINTEXTRACTION_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui { class LinePointExtraction_Widget; }

class LinePointExtraction_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit LinePointExtraction_Widget(QWidget *parent = nullptr);
    ~LinePointExtraction_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onMethodChanged(int index);
    void onParameterChanged();

private:
    Ui::LinePointExtraction_Widget *ui;
    
    void setupMethodComboBox();
    void updateMethodDescription();
    void validateParameters();
};

#endif // LINEPOINTEXTRACTION_WIDGET_HPP 