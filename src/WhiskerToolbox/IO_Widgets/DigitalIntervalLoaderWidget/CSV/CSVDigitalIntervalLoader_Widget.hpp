#ifndef CSV_DIGITAL_INTERVAL_LOADER_WIDGET_HPP
#define CSV_DIGITAL_INTERVAL_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>

namespace Ui {
class CSVDigitalIntervalLoader_Widget;
}

class CSVDigitalIntervalLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVDigitalIntervalLoader_Widget(QWidget *parent = nullptr);
    ~CSVDigitalIntervalLoader_Widget() override;

signals:
    void loadFileRequested(QString delimiterText);

private:
    Ui::CSVDigitalIntervalLoader_Widget *ui;
};

#endif // CSV_DIGITAL_INTERVAL_LOADER_WIDGET_HPP 