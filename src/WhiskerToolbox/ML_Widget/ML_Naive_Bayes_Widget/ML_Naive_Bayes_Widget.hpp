#ifndef ML_NAIVE_BAYES_WIDGET_HPP
#define ML_NAIVE_BAYES_WIDGET_HPP

#include "ML_Widget/MLParameterWidgetBase.hpp"
#include "ML_Widget/MLModelParameters.hpp"

#include <memory>

class DataManager;

namespace Ui {
class ML_Naive_Bayes_Widget;
}

class ML_Naive_Bayes_Widget : public MLParameterWidgetBase {
    Q_OBJECT

public:
    explicit ML_Naive_Bayes_Widget(std::shared_ptr<DataManager> data_manager,
                                   QWidget * parent = nullptr);

    ~ML_Naive_Bayes_Widget() override;

    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getParameters() const override;

protected:
private slots:

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::ML_Naive_Bayes_Widget * ui;
};


#endif// ML_NAIVE_BAYES_WIDGET_HPP
