#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "DataManager/DataManagerTypes.hpp"

#include <QMainWindow>

#include "DockManager.h"

#include <map>
#include <memory>
#include <string>

class DataManager;
class DataManager_Widget;
class GroupManager;
class GroupManagementWidget;
class MediaWidgetManager;


namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget * parent = nullptr);
    ~MainWindow() override;

    void addWidget(std::string const & key, std::unique_ptr<QWidget> widget) {
        _widgets[key] = std::move(widget);
    }

    QWidget * getWidget(std::string const & key) {
        auto it = _widgets.find(key);
        if (it != _widgets.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    void registerDockWidget(std::string const & key, QWidget * widget, ads::DockWidgetArea area);
    void showDockWidget(std::string const & key);

    /**
     * @brief Access the global dock manager used by the application
     * @return Pointer to the ads::CDockManager managing docks
     */
    ads::CDockManager * dockManager() const { return _m_DockManager; }

    /**
     * @brief Find the ADS dock widget by key
     * @param key Dock key used at registration
     * @return Pointer to the dock widget or nullptr if not found
     */
    ads::CDockWidget * findDockWidget(std::string const & key) const;

    /**
     * @brief Get the global group manager
     * @return Pointer to the GroupManager
     */
    GroupManager * getGroupManager() const { return _group_manager.get(); }

    void processLoadedData(std::vector<DataInfo> const & data_info);


protected:
    void keyPressEvent(QKeyEvent * event) override;
    bool eventFilter(QObject * obj, QEvent * event) override;

private:
    Ui::MainWindow * ui;

    std::unique_ptr<MediaWidgetManager> _media_manager;

    ads::CDockManager * _m_DockManager;

    bool _verbose;

    std::shared_ptr<DataManager> _data_manager;

    std::unique_ptr<GroupManager> _group_manager;
    GroupManagementWidget * _group_management_widget;
    DataManager_Widget * _data_manager_widget;

    // Main widgets created programmatically (not in UI file)
    class Media_Widget * _media_widget;
    class TimeScrollBar * _time_scrollbar;

    std::map<std::string, std::unique_ptr<QWidget>> _widgets;
    
    // Counter for generating unique media widget IDs
    int _media_widget_counter = 1; // Start at 1 since "main" is the initial one

    void _createActions();
    void _buildInitialLayout();

    void loadData();
    void _updateFrameCount();

private slots:
    void Load_Video();
    void Load_Images();

    void _loadJSONConfig();

    void openWhiskerTracking();
    void openTongueTracking();
    void openMLWidget();
    void openDataViewer();
    void openNewMediaWidget();
    void openBatchProcessingWidget();
    void openPointLoaderWidget();
    void openMaskLoaderWidget();
    void openLineLoaderWidget();
    void openIntervalLoaderWidget();
    void openEventLoaderWidget();
    void openTensorLoaderWidget();
    void openDataManager();
    void openGroupManagement();
    void openVideoExportWidget();
    void openDataTransforms();
    void openTerminalWidget();
    void openAnalysisDashboard();
    void openTestWidget();
    void openTableDesignerWidget();
};
#endif// MAINWINDOW_HPP
