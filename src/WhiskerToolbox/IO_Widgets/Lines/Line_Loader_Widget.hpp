#ifndef LINE_LOADER_WIDGET_HPP
#define LINE_LOADER_WIDGET_HPP

#include "../Scaling_Widget/Scaling_Widget.hpp"
#include "CoreGeometry/lines.hpp"
#include "DataManager/TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <map>
#include <vector>

// Forward declarations
namespace Ui {
class Line_Loader_Widget;
}
class DataManager;
class QComboBox;
class QStackedWidget;
class BinaryLineLoader_Widget;

// Forward declare the CSV options structs
struct CSVSingleFileLineLoaderOptions;
struct CSVMultiFileLineLoaderOptions;

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
    void _loadCSVData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data_map, std::string const & base_key);

private slots:
    void _loadSingleHdf5Line(QString filename);
    void _loadMultiHdf5Line(QString dir_name, QString pattern);
    void _onLoaderTypeChanged(int index);
    void _handleLoadBinaryFileRequested(QString filepath);
    void _handleLoadSingleFileCSVRequested(CSVSingleFileLineLoaderOptions options);
    void _handleLoadMultiFileCSVRequested(CSVMultiFileLineLoaderOptions options);
};


#endif// LINE_LOADER_WIDGET_HPP
