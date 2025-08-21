#ifndef LINE_WIDGET_HPP
#define LINE_WIDGET_HPP

// Remove direct IO dependencies - use JSON registry pattern
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "MediaExport/MediaExport_Widget.hpp"

#include <QModelIndex>
#include <QWidget>
#include <QString>
#include "nlohmann/json.hpp"

#include <memory>
#include <string>

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

// JSON-based saver configuration - no need for variant types
using LineSaverConfig = nlohmann::json;

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

    // No longer need SaverType enum - use format strings directly

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
    void _handleSaveCSVRequested(QString format, nlohmann::json config);
    void _handleSaveMultiFileCSVRequested(QString format, nlohmann::json config);
    void _handleSaveBinaryRequested(QString format, nlohmann::json config);
    void _onExportMediaFramesCheckboxToggled(bool checked);

private:
    void _initiateSaveProcess(QString const& format, LineSaverConfig const& config);
    bool _performRegistrySave(QString const& format, LineSaverConfig const& config);

    /**
     * @brief Get selected frames from the table view
     * 
     * @return Vector of unique frame numbers that are currently selected
     */
    std::vector<TimeFrameIndex> _getSelectedFrames();
};

#endif// LINE_WIDGET_HPP
