#ifndef TIMEFRAME_TABLE_WIDGET_HPP
#define TIMEFRAME_TABLE_WIDGET_HPP

#include <QWidget>

#include <memory>

namespace Ui {
class TimeFrame_Table_Widget;
}

class DataManager;

/**
 * @brief Widget displaying all TimeFrame objects registered in the DataManager
 *
 * Shows a two-column table with the TimeFrame name and its sample count.
 * Automatically refreshes when the DataManager notifies observers (e.g. when
 * new TimeFrames are added or existing ones are updated).
 */
class TimeFrame_Table_Widget : public QWidget {
    Q_OBJECT

public:
    explicit TimeFrame_Table_Widget(QWidget * parent = nullptr);
    ~TimeFrame_Table_Widget() override;

    /**
     * @brief Set the DataManager and subscribe to its observer notifications
     * @param data_manager Shared pointer to the DataManager
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Rebuild the table from the current DataManager state
     */
    void populateTable();

protected:
    void resizeEvent(QResizeEvent * event) override;

private slots:
    void _refreshTable();

private:
    Ui::TimeFrame_Table_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    bool _is_resizing{false};

    void _setAdaptiveColumnWidths();
};

#endif // TIMEFRAME_TABLE_WIDGET_HPP
