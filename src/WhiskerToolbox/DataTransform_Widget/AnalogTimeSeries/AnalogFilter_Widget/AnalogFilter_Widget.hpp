#ifndef ANALOG_FILTER_WIDGET_HPP
#define ANALOG_FILTER_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui { class AnalogFilter_Widget; }

class AnalogFilter_Widget : public TransformParameter_Widget {
    Q_OBJECT

public:
    explicit AnalogFilter_Widget(QWidget * parent = nullptr);
    ~AnalogFilter_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void _onFilterTypeChanged(int index);
    void _onResponseChanged(int index);
    void _validateParameters();
    void _updateVisibleParameters();

private:
    std::unique_ptr<Ui::AnalogFilter_Widget> ui;
    void _setupConnections();
}; 

#endif // ANALOG_FILTER_WIDGET_HPP