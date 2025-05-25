#ifndef CSV_ANALOG_SAVER_WIDGET_HPP
#define CSV_ANALOG_SAVER_WIDGET_HPP

#include <QWidget>
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp" // For CSVAnalogSaverOptions

// Forward declaration
namespace Ui {
class CSVAnalogSaver_Widget;
}

class CSVAnalogSaver_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVAnalogSaver_Widget(QWidget *parent = nullptr);
    ~CSVAnalogSaver_Widget() override;

signals:
    void saveAnalogCSVRequested(CSVAnalogSaverOptions options);

private slots:
    void _onSaveHeaderCheckboxToggled(bool checked);
    void _updatePrecisionExample(int precision);

private:
    Ui::CSVAnalogSaver_Widget *ui;
    void _updatePrecisionLabelText(int precision);
};

#endif // CSV_ANALOG_SAVER_WIDGET_HPP 