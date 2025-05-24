#ifndef WHISKERTOOLBOX_MLPARAMETERWIDGETBASE_HPP
#define WHISKERTOOLBOX_MLPARAMETERWIDGETBASE_HPP

#include <QWidget>
#include <memory>
#include "ML_Widget/MLModelParameters.hpp" // For MLModelParametersBase

class MLParameterWidgetBase : public QWidget {
    Q_OBJECT
public:
    using QWidget::QWidget; // Inherit QWidget constructors
    virtual ~MLParameterWidgetBase() = default;
    [[nodiscard]] virtual std::unique_ptr<MLModelParametersBase> getParameters() const = 0;
};

#endif // WHISKERTOOLBOX_MLPARAMETERWIDGETBASE_HPP 