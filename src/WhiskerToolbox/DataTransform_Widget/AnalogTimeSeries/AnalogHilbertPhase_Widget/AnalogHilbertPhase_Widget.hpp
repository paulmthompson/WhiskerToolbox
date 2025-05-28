#ifndef ANALOGHILBERTPHASE_WIDGET_HPP
#define ANALOGHILBERTPHASE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/data_transforms.hpp"

namespace Ui { class AnalogHilbertPhase_Widget; }

class AnalogHilbertPhase_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit AnalogHilbertPhase_Widget(QWidget *parent = nullptr);
    ~AnalogHilbertPhase_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void _validateFrequencyParameters();
    void _validateParameters();

private:
    Ui::AnalogHilbertPhase_Widget *ui;
};

#endif// ANALOGHILBERTPHASE_WIDGET_HPP
