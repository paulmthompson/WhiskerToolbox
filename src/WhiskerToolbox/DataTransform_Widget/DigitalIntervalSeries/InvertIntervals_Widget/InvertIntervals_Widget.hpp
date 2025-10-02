#ifndef INVERTINTERVALS_WIDGET_HPP
#define INVERTINTERVALS_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui {
class InvertIntervals_Widget;
}

class InvertIntervals_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit InvertIntervals_Widget(QWidget * parent = nullptr);
    ~InvertIntervals_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onDomainTypeChanged();
    void validateBounds();

private:
    Ui::InvertIntervals_Widget * ui;
};

#endif// INVERTINTERVALS_WIDGET_HPP
