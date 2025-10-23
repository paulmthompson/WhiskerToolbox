#ifndef BOOLEANOPERATION_WIDGET_HPP
#define BOOLEANOPERATION_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

#include <string>

namespace Ui {
class BooleanOperation_Widget;
}

class BooleanOperation_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit BooleanOperation_Widget(QWidget * parent = nullptr);
    ~BooleanOperation_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private slots:
    void _operationChanged(int index);
    void _otherSeriesChanged(int index);

private:
    Ui::BooleanOperation_Widget * ui;
    std::string _selected_other_series_key;
    std::string _current_input_key; // Track the currently selected input feature

    void _refreshOtherSeriesKeys();
    void _updateOtherSeriesComboBox();
    void _updateOperationDescription();

public:
    // Allow setting the current input key so we can filter it out
    void setCurrentInputKey(std::string const & key) {
        _current_input_key = key;
        _refreshOtherSeriesKeys();
    }
};

#endif// BOOLEANOPERATION_WIDGET_HPP
