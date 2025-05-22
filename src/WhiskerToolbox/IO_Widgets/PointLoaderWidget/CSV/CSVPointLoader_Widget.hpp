#ifndef CSV_POINT_LOADER_WIDGET_HPP
#define CSV_POINT_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"

namespace Ui {
class CSVPointLoader_Widget;
}

class CSVPointLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVPointLoader_Widget(QWidget *parent = nullptr);
    ~CSVPointLoader_Widget() override;

signals:
    // Changed signature to pass CSVPointLoaderOptions struct
    void loadSingleCSVFileRequested(CSVPointLoaderOptions options);

private:
    Ui::CSVPointLoader_Widget *ui;
};

#endif // CSV_POINT_LOADER_WIDGET_HPP 