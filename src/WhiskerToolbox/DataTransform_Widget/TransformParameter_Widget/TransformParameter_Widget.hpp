#ifndef TRANSFORMPARAMETER_WIDGET_HPP
#define TRANSFORMPARAMETER_WIDGET_HPP

#include <QWidget>
#include <memory>

class TransformParametersBase;

class TransformParameter_Widget : public QWidget {
    Q_OBJECT
public:
    using QWidget::QWidget;
    virtual ~TransformParameter_Widget() = default;
    virtual std::unique_ptr<TransformParametersBase> getParameters() const = 0;

};

#endif// TRANSFORMPARAMETER_WIDGET_HPP
