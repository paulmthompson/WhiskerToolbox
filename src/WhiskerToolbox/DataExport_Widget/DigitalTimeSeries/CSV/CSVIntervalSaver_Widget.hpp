#ifndef CSV_INTERVAL_SAVER_WIDGET_HPP
#define CSV_INTERVAL_SAVER_WIDGET_HPP

#include <QWidget>
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"

// Forward declaration of the UI class
namespace Ui {
class CSVIntervalSaver_Widget;
}

class CSVIntervalSaver_Widget : public QWidget {
    Q_OBJECT

public:
    explicit CSVIntervalSaver_Widget(QWidget *parent = nullptr);
    ~CSVIntervalSaver_Widget() override;

signals:
    void saveIntervalCSVRequested(CSVIntervalSaverOptions options);

private slots:
    void _onSaveHeaderCheckboxToggled(bool checked);
    void _onSaveActionButtonClicked();

private:
    Ui::CSVIntervalSaver_Widget *ui;
    CSVIntervalSaverOptions _getOptionsFromUI() const;
};

#endif // CSV_INTERVAL_SAVER_WIDGET_HPP 