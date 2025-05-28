#ifndef CSV_DIGITAL_EVENT_LOADER_WIDGET_HPP
#define CSV_DIGITAL_EVENT_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"

namespace Ui {
class CSVDigitalEventLoader_Widget;
}

class CSVDigitalEventLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVDigitalEventLoader_Widget(QWidget *parent = nullptr);
    ~CSVDigitalEventLoader_Widget() override;

signals:
    void loadCSVEventRequested(CSVEventLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onIdentifierCheckboxToggled(bool checked);

private:
    Ui::CSVDigitalEventLoader_Widget *ui;
    void _updateUIForIdentifierMode();
};

#endif // CSV_DIGITAL_EVENT_LOADER_WIDGET_HPP 