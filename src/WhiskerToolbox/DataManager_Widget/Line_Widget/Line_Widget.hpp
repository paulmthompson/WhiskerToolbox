#ifndef LINE_WIDGET_HPP
#define LINE_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

#include <QModelIndex> // Required for signal/slot

// Forward declarations
namespace Ui {
class Line_Widget;
}
class DataManager;
class LineTableModel;

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

private slots:
    // Add any slots needed for handling user interactions
    void _handleCellDoubleClicked(QModelIndex const & index); // Slot for table interaction
    void _onDataChanged(); // Slot for DataManager callback
    void _moveLineButton_clicked(); // Slot for move line button
    void _deleteLineButton_clicked(); // Slot for delete line button

private:
    void _populateMoveToComboBox(); // Method to populate the combo box
};

#endif// LINE_WIDGET_HPP
