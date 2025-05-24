#ifndef ML_RANDOM_FOREST_WIDGET_HPP
#define ML_RANDOM_FOREST_WIDGET_HPP

#include "ML_Widget/MLParameterWidgetBase.hpp"
#include "ML_Widget/MLModelParameters.hpp"

#include <memory>

class DataManager;

namespace Ui {
class ML_Random_Forest_Widget;
}

class ML_Random_Forest_Widget : public MLParameterWidgetBase {
    Q_OBJECT

public:
    explicit ML_Random_Forest_Widget(std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent = nullptr);

    ~ML_Random_Forest_Widget() override;

    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getParameters() const override;

protected:
private slots:

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::ML_Random_Forest_Widget * ui;
};


#endif// ML_RANDOM_FOREST_WIDGET_HPP
