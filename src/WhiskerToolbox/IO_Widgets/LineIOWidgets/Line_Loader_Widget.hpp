#ifndef LINE_LOADER_WIDGET_HPP
#define LINE_LOADER_WIDGET_HPP

#include "../Scaling_Widget/Scaling_Widget.hpp"
#include <QWidget>

#include <memory>
#include <string>

// Forward declarations
namespace Ui {
class Line_Loader_Widget;
}
class DataManager;
class QComboBox;
class QStackedWidget;
class BinaryLineLoader_Widget;

class Line_Loader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Line_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Line_Loader_Widget() override;

private:
    Ui::Line_Loader_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    void _loadSingleHDF5Line(std::string const & filename, std::string const & line_suffix = "");
    void _loadSingleBinaryFile(QString const & filepath);

private slots:
    void _loadSingleHdf5Line(QString filename);
    void _loadMultiHdf5Line(QString dir_name, QString pattern);
    void _onLoaderTypeChanged(int index);
    void _handleLoadBinaryFileRequested(QString filepath);
};


#endif// LINE_LOADER_WIDGET_HPP
