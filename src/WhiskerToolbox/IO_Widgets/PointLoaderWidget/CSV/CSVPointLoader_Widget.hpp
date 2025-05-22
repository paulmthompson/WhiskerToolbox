#ifndef CSV_POINT_LOADER_WIDGET_HPP
#define CSV_POINT_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>

namespace Ui {
class CSVPointLoader_Widget;
}

class CSVPointLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVPointLoader_Widget(QWidget *parent = nullptr);
    ~CSVPointLoader_Widget() override;

signals:
    // The delimiter char will be determined from the combobox text
    void loadSingleCSVFileRequested(QString delimiterText);

private:
    Ui::CSVPointLoader_Widget *ui;
};

#endif // CSV_POINT_LOADER_WIDGET_HPP 