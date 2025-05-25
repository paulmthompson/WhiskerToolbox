#ifndef CSV_POINT_SAVER_WIDGET_HPP
#define CSV_POINT_SAVER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"

namespace Ui {
class CSVPointSaver_Widget;
}

class CSVPointSaver_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVPointSaver_Widget(QWidget *parent = nullptr);
    ~CSVPointSaver_Widget() override;

signals:
    void saveCSVRequested(CSVPointSaverOptions options);

private slots:
    void _onSaveHeaderCheckboxToggled(bool checked);

private:
    Ui::CSVPointSaver_Widget *ui;
};

#endif // CSV_POINT_SAVER_WIDGET_HPP 
