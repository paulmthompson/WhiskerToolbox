#ifndef POINT_WIDGET_HPP
#define POINT_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>
#include <variant>

#include <QModelIndex>
#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"

namespace Ui {
class Point_Widget;
}

class DataManager;
class PointTableModel;
class CSVPointSaver_Widget;
class MediaData;

using PointSaverOptionsVariant = std::variant<CSVPointSaverOptions>;

class Point_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Point_Widget() override;

    void openWidget();// Call to open the widget

    void setActiveKey(std::string const & key);

    void updateTable();

    void loadFrame(int frame_id);

    void removeCallbacks();

signals:
    void frameSelected(int frame_id);

private:
    Ui::Point_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    PointTableModel * _point_table_model;
    std::string _active_key;
    int _previous_frame{0};
    int _callback_id{-1};
    enum SaverType { CSV };

    //void refreshTable();
    void _propagateLabel(int frame_id);
    void _populateMoveToPointDataComboBox();
    void _saveToCSVFile(CSVPointSaverOptions & options);
    bool _performActualCSVSave(CSVPointSaverOptions & options);
    void _initiateSaveProcess(SaverType saver_type, PointSaverOptionsVariant& options_variant);


private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _movePointsButton_clicked();
    void _deletePointsButton_clicked();
    void _onDataChanged();
    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(CSVPointSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);
};

#endif// POINT_WIDGET_HPP
