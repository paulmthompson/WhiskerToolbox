/**
 * @file NewTimeFrameWidget.hpp
 * @brief Widget for creating new TimeFrames (e.g., upsampled from existing ones).
 */

#ifndef NEWTIMEFRAMEWIDGET_HPP
#define NEWTIMEFRAMEWIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class NewTimeFrameWidget;
}

class DataManager;

class NewTimeFrameWidget : public QWidget {
    Q_OBJECT
public:
    explicit NewTimeFrameWidget(QWidget * parent = nullptr);
    ~NewTimeFrameWidget() override;

    /**
     * @brief Set the DataManager instance for this widget
     * @param data_manager Shared pointer to the DataManager instance
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Populate the source timeframe combo box with available timeframes
     */
    void populateTimeframes();

signals:
    void createTimeFrame(std::string name, std::string source_timeframe_key, int upsampling_factor);

private:
    Ui::NewTimeFrameWidget * ui;
    std::shared_ptr<DataManager> _data_manager;
    QString _last_auto_name;

    void _updatePreview();
    void _autoSuggestName();

private slots:
    void _createTimeFrame();
};

#endif// NEWTIMEFRAMEWIDGET_HPP
