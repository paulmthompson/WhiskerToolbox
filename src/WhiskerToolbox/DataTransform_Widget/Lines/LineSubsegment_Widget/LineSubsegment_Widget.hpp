#ifndef LINESUBSEGMENT_WIDGET_HPP
#define LINESUBSEGMENT_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Lines/line_subsegment.hpp"

namespace Ui { class LineSubsegment_Widget; }

class LineSubsegment_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit LineSubsegment_Widget(QWidget *parent = nullptr);
    ~LineSubsegment_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onMethodChanged(int index);
    void onParameterChanged();

private:
    Ui::LineSubsegment_Widget *ui;
    
    void setupMethodComboBox();
    void updateMethodDescription();
    void validateParameters();
};

#endif // LINESUBSEGMENT_WIDGET_HPP 