#ifndef CSVANALOGLOADER_WIDGET_HPP
#define CSVANALOGLOADER_WIDGET_HPP

#include <QWidget>
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"

namespace Ui {
class CSVAnalogLoader_Widget;
}

class CSVAnalogLoader_Widget : public QWidget {
    Q_OBJECT

public:
    explicit CSVAnalogLoader_Widget(QWidget *parent = nullptr);
    ~CSVAnalogLoader_Widget() override;

signals:
    void loadAnalogCSVRequested(CSVAnalogLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onSingleColumnCheckboxToggled(bool checked);

private:
    Ui::CSVAnalogLoader_Widget *ui;
    void _updateColumnControlsState();
};

#endif // CSVANALOGLOADER_WIDGET_HPP 