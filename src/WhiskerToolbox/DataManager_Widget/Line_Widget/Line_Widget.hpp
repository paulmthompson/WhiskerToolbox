#ifndef LINE_WIDGET_HPP
#define LINE_WIDGET_HPP

#include "DataManager/Lines/IO/Binary/Line_Data_Binary.hpp"
#include "DataManager/Lines/IO/CSV/Line_Data_CSV.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "IO_Widgets/Media/MediaExport_Widget.hpp"

#include <QModelIndex>
#include <QWidget>

#include <memory>
#include <string>
#include <variant>

namespace Ui {
class Line_Widget;
}
class DataManager;
class LineTableModel;
class CSVLineSaver_Widget;
class BinaryLineSaver_Widget;
class QStackedWidget;
class QComboBox;
class QCheckBox;

using LineSaverOptionsVariant = std::variant<CSVSingleFileLineSaverOptions, CSVMultiFileLineSaverOptions, BinaryLineSaverOptions>;

class Line_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Line_Widget() override;

    void openWidget();
    void setActiveKey(std::string const & key);
    void removeCallbacks();
    void updateTable();

signals:
    void frameSelected(int frame_id);

private:
    Ui::Line_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    LineTableModel * _line_table_model;
    std::string _active_key;
    int _callback_id{-1};

    enum SaverType { CSV,
                     BINARY };

    /**
     * @brief Move selected line to the specified target key
     * 
     * @param target_key The key to move the line to
     */
    void _moveLineToTarget(std::string const & target_key);

    /**
     * @brief Copy selected line to the specified target key
     * 
     * @param target_key The key to copy the line to
     */
    void _copyLineToTarget(std::string const & target_key);

    /**
     * @brief Show context menu for the table view
     * 
     * @param position The position where the context menu should appear
     */
    void _showContextMenu(QPoint const & position);

private slots:
    void _handleCellDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _deleteSelectedLine();

    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(CSVSingleFileLineSaverOptions options);
    void _handleSaveMultiFileCSVRequested(CSVMultiFileLineSaverOptions options);
    void _handleSaveBinaryRequested(BinaryLineSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);

private:
    void _initiateSaveProcess(SaverType saver_type, LineSaverOptionsVariant & options_variant);
    bool _performActualCSVSave(CSVSingleFileLineSaverOptions & options);
    bool _performActualMultiFileCSVSave(CSVMultiFileLineSaverOptions & options);
    bool _performActualBinarySave(BinaryLineSaverOptions & options);

    /**
     * @brief Get selected frames from the table view
     * 
     * @return Vector of unique frame numbers that are currently selected
     */
    std::vector<TimeFrameIndex> _getSelectedFrames();
};

#endif// LINE_WIDGET_HPP
