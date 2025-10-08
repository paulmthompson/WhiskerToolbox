#ifndef POINT_WIDGET_HPP
#define POINT_WIDGET_HPP

#include <QWidget>

#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"         // For CSVPointSaverOptions
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"// For context menu utilities
#include "MediaExport/MediaExport_Widget.hpp"              // For MediaExport_Widget
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <string>
#include <variant>// std::variant

namespace Ui {
class Point_Widget;
}

class DataManager;
class PointTableModel;
class CSVPointSaver_Widget;
class MediaData;

// Define the variant type for saver options
using PointSaverOptionsVariant = std::variant<CSVPointSaverOptions>;

/**
 * @brief Point Widget Class
 *
 * This widget is used for visualizing and managing point data in a table view.
 * It provides functionality to save point data to various formats and export
 * matching media frames.
 *
 * Point data is organized by time (frame), with each frame potentially containing
 * multiple points. The widget allows for moving and copying points between different
 * PointData instances via right-click context menu.
 */
class Point_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Point_Widget() override;

    void openWidget();
    void setActiveKey(std::string const & key);

    void updateTable();

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

    void _saveToCSVFile(CSVPointSaverOptions & options);
    bool _performActualCSVSave(CSVPointSaverOptions & options);
    void _initiateSaveProcess(SaverType saver_type, PointSaverOptionsVariant & options_variant);

    /**
     * @brief Get frame numbers from selected table rows
     * 
     * @return Vector of frame numbers corresponding to selected rows
     */
    std::vector<TimeFrameIndex> _getSelectedFrames();

    /**
     * @brief Move selected points to the specified target key
     * 
     * @param target_key The key to move points to
     */
    void _movePointsToTarget(std::string const & target_key);

    /**
     * @brief Copy selected points to the specified target key
     * 
     * @param target_key The key to copy points to
     */
    void _copyPointsToTarget(std::string const & target_key);

    /**
     * @brief Show context menu for the table view
     * 
     * @param position The position where the context menu should appear
     */
    void _showContextMenu(QPoint const & position);

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(CSVPointSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);
    void _deleteSelectedPoints();
    void _onApplyImageSizeClicked();
    void _onCopyImageSizeClicked();
    void _updateImageSizeDisplay();
    void _populateMediaComboBox();
};

#endif// POINT_WIDGET_HPP
