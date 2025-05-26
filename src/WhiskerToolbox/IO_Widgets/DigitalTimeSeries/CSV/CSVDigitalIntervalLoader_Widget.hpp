#ifndef CSV_DIGITAL_INTERVAL_LOADER_WIDGET_HPP
#define CSV_DIGITAL_INTERVAL_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"

namespace Ui {
class CSVDigitalIntervalLoader_Widget;
}

class CSVDigitalIntervalLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVDigitalIntervalLoader_Widget(QWidget *parent = nullptr);
    ~CSVDigitalIntervalLoader_Widget() override;

signals:
    void loadCSVIntervalRequested(CSVIntervalLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();

private:
    Ui::CSVDigitalIntervalLoader_Widget *ui;
};

#endif // CSV_DIGITAL_INTERVAL_LOADER_WIDGET_HPP 