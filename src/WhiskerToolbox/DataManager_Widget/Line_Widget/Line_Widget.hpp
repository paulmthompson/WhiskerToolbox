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
#include "IO_Widgets/Media/MediaExport_Widget.hpp" // For MediaExport_Widget

// Forward declarations
namespace Ui {
class Line_Widget;
}
class DataManager;
class LineTableModel;
class CSVLineSaver_Widget; // Forward declare the new widget

// Define the variant type for saver options
using LineSaverOptionsVariant = std::variant<CSVSingleFileLineSaverOptions>;

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

    enum SaverType { CSV }; // Enum for different saver types

private slots:
    // Add any slots needed for handling user interactions
    void _handleCellDoubleClicked(QModelIndex const & index); // Slot for table interaction
    void _onDataChanged(); // Slot for DataManager callback
    void _moveLineButton_clicked(); // Slot for move line button
    void _deleteLineButton_clicked(); // Slot for delete line button

    // New slots for saving functionality
    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(CSVSingleFileLineSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);

private:
    void _populateMoveToComboBox(); // Method to populate the combo box

    // New private helper methods for saving
    void _initiateSaveProcess(SaverType saver_type, LineSaverOptionsVariant& options_variant);
    bool _performActualCSVSave(CSVSingleFileLineSaverOptions & options);
};

#endif// LINE_WIDGET_HPP
