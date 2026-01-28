#ifndef CSV_EVENT_SAVER_WIDGET_HPP
#define CSV_EVENT_SAVER_WIDGET_HPP

#include <QWidget>
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"

// Forward declaration of the UI class
namespace Ui {
class CSVEventSaver_Widget;
}

class CSVEventSaver_Widget : public QWidget {
    Q_OBJECT

public:
    explicit CSVEventSaver_Widget(QWidget *parent = nullptr);
    ~CSVEventSaver_Widget() override;

signals:
    void saveEventCSVRequested(CSVEventSaverOptions options);

private slots:
    void _onSaveHeaderCheckboxToggled(bool checked);
    void _onSaveActionButtonClicked();

private:
    Ui::CSVEventSaver_Widget *ui;
    CSVEventSaverOptions _getOptionsFromUI() const;
};

#endif // CSV_EVENT_SAVER_WIDGET_HPP
