#ifndef POINT_LOADER_WIDGET_HPP
#define POINT_LOADER_WIDGET_HPP

#include "IO_Widgets/PointLoaderWidget/CSV/CSVPointLoader_Widget.hpp"

#include "IO_Widgets/Scaling_Widget/Scaling_Widget.hpp"

#include <QWidget>

#include <memory>


class DataManager;

namespace Ui {
class Point_Loader_Widget;
}

class Point_Loader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Point_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Point_Loader_Widget() override;

private:
    Ui::Point_Loader_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    void _loadSingleCSVFile(std::string const& filename, QString delimiterText);

private slots:
    void _onLoaderTypeChanged(int index);
    void _handleSingleCSVLoadRequested(QString delimiterText);
};

#endif// POINT_LOADER_WIDGET_HPP
