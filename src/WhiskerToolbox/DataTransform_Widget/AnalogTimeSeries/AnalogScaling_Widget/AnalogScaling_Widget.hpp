#ifndef ANALOGSCALING_WIDGET_HPP
#define ANALOGSCALING_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"
#include "DataManager/transforms/AnalogTimeSeries/Analog_Scaling/analog_scaling.hpp" // Analog Statistics

class DataManager;
namespace Ui { class AnalogScaling_Widget; }

class AnalogScaling_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit AnalogScaling_Widget(QWidget *parent = nullptr);
    ~AnalogScaling_Widget() override;

    void setCurrentDataKey(QString const & data_key);
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private slots:
    void onMethodChanged(int index);
    void onParameterChanged();
    void updateStatistics();

private:
    Ui::AnalogScaling_Widget *ui;
    QString _current_data_key;
    AnalogStatistics _current_stats;
    
    void setupMethodComboBox();
    void updateParameterVisibility();
    void updateMethodDescription();
    void validateParameters();
    void displayStatistics(AnalogStatistics const & stats);
    QString formatNumber(double value, int precision = 4) const;
};

#endif // ANALOGSCALING_WIDGET_HPP
