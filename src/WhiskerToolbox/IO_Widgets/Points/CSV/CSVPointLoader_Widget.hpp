#ifndef CSV_POINT_LOADER_WIDGET_HPP
#define CSV_POINT_LOADER_WIDGET_HPP

#include <QWidget>

struct CSVPointLoaderOptions;

namespace Ui {
class CSVPointLoader_Widget;
}

class CSVPointLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVPointLoader_Widget(QWidget * parent = nullptr);
    ~CSVPointLoader_Widget() override;

signals:

    void loadSingleCSVFileRequested(CSVPointLoaderOptions const & options);

private:
    Ui::CSVPointLoader_Widget * ui;
};

#endif// CSV_POINT_LOADER_WIDGET_HPP