#ifndef LINE_WIDGET_HPP
#define LINE_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>
#include <variant> // Required for std::variant

#include <QModelIndex> // Required for signal/slot

// Forward declarations of Qt classes if not fully included
class QStackedWidget;
class QComboBox;
class QCheckBox;

#include "DataManager/Lines/IO/CSV/Line_Data_CSV.hpp" // For CSVSingleFileLineSaverOptions
#include "DataManager/Lines/IO/Binary/Line_Data_Binary.hpp" // For BinaryLineSaverOptions
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp" // For context menu utilities
#include "IO_Widgets/Media/MediaExport_Widget.hpp" // For MediaExport_Widget

// Forward declarations
namespace Ui {
class Line_Widget;
}
class DataManager;
class LineTableModel;
class CSVLineSaver_Widget; // Forward declare the new widget
class BinaryLineSaver_Widget; // Forward declare the binary saver widget

// Define the variant type for saver options
using LineSaverOptionsVariant = std::variant<CSVSingleFileLineSaverOptions, CSVMultiFileLineSaverOptions, BinaryLineSaverOptions>;

class Line_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Line_Widget() override;

    void openWidget();// Call to open the widget
    void setActiveKey(std::string const & key);
    void removeCallbacks(); // Added to manage callbacks
    void updateTable(); // Added to refresh table data

signals:
    void frameSelected(int frame_id);

private:
    Ui::Line_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    LineTableModel * _line_table_model; // Added table model
    std::string _active_key;            // Added to store active key
    int _callback_id{-1};             // Added for data manager callback

    enum SaverType { CSV, BINARY }; // Enum for different saver types

    /**
     * @brief Move selected line to the specified target key
     * 
     * @param target_key The key to move the line to
     */
    void _moveLineToTarget(std::string const& target_key);

    /**
     * @brief Copy selected line to the specified target key
     * 
     * @param target_key The key to copy the line to
     */
    void _copyLineToTarget(std::string const& target_key);

    /**
     * @brief Show context menu for the table view
     * 
     * @param position The position where the context menu should appear
     */
    void _showContextMenu(QPoint const& position);

private slots:
    // Add any slots needed for handling user interactions
    void _handleCellDoubleClicked(QModelIndex const & index); // Slot for table interaction
    void _onDataChanged(); // Slot for DataManager callback
    void _deleteSelectedLine(); // Slot for delete line operation

    // New slots for saving functionality
    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(CSVSingleFileLineSaverOptions options);
    void _handleSaveMultiFileCSVRequested(CSVMultiFileLineSaverOptions options);
    void _handleSaveBinaryRequested(BinaryLineSaverOptions options); // New slot for binary save
    void _onExportMediaFramesCheckboxToggled(bool checked);

private:
    // New private helper methods for saving
    void _initiateSaveProcess(SaverType saver_type, LineSaverOptionsVariant& options_variant);
    bool _performActualCSVSave(CSVSingleFileLineSaverOptions & options);
    bool _performActualMultiFileCSVSave(CSVMultiFileLineSaverOptions & options);
    bool _performActualBinarySave(BinaryLineSaverOptions & options); // New helper for binary save

    /**
     * @brief Get selected frames from the table view
     * 
     * @return Vector of unique frame numbers that are currently selected
     */
    std::vector<int> _getSelectedFrames();
};

#endif// LINE_WIDGET_HPP
