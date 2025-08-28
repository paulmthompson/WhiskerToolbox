#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "DataManager/DataManagerTypes.hpp"

#include <QMainWindow>

#include "DockManager.h"

#include <map>
#include <memory>
#include <string>

class DataManager;
class MediaDisplayCoordinator;
class MediaDisplayManager;


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

    void processLoadedData(std::vector<DataInfo> const & data_info);

    /**
     * @brief Get the media display coordinator for creating/managing media displays
     */
    MediaDisplayCoordinator* getMediaDisplayCoordinator() const { return _media_coordinator.get(); }

    /**
     * @brief Create a new media display and dock it
     * @param dock_name Name for the dock widget
     * @param area Dock area to place the new display
     * @return Pointer to the created MediaDisplayManager
     */
    MediaDisplayManager* createMediaDisplay(const std::string& dock_name = "media", 
                                           ads::DockWidgetArea area = ads::TopDockWidgetArea);


protected:
    void keyPressEvent(QKeyEvent * event) override;
    bool eventFilter(QObject * obj, QEvent * event) override;

private:
    Ui::MainWindow * ui;

    std::unique_ptr<MediaDisplayCoordinator> _media_coordinator;

    ads::CDockManager * _m_DockManager;

    bool _verbose;

    std::shared_ptr<DataManager> _data_manager;

    std::map<std::string, std::unique_ptr<QWidget>> _widgets;

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
    void openBatchProcessingWidget();
    void openPointLoaderWidget();
    void openMaskLoaderWidget();
    void openLineLoaderWidget();
    void openIntervalLoaderWidget();
    void openEventLoaderWidget();
    void openTensorLoaderWidget();
    void openDataManager();
    void openVideoExportWidget();
    void openSpreadsheetExportWidget();
    void openEnhancedMediaExportWidget(); // NEW: Enhanced export widget
    void openDataTransforms();
    void openTerminalWidget();
    void openAnalysisDashboard();
    void openTestWidget();
    void openTableDesignerWidget();
    
    // NEW: Slots for managing multiple media displays
    void createNewMediaDisplay();
    void _onMediaDisplayCreated(const std::string& display_id);
    void _onActiveDisplayChanged(const std::string& display_id);
};
#endif// MAINWINDOW_HPP
