#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

#include "DockManager.h"

#include <map>
#include <memory>
#include <string>

class DataManager;
class Media_Window;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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


protected:
    void keyPressEvent(QKeyEvent * event) override;

private:
    Ui::MainWindow * ui;

    Media_Window * _scene;

    ads::CDockManager * _m_DockManager;

    bool _verbose;

    std::shared_ptr<DataManager> _data_manager;

    std::map<std::string, std::unique_ptr<QWidget>> _widgets;

    void _createActions();
    void _buildInitialLayout();

    void _LoadData();
    void _updateFrameCount();

private slots:
    void Load_Video();
    void Load_Images();

    void _loadDigitalTimeSeriesCSV();

    void _loadJSONConfig();

    void openWhiskerTracking();
    void openLabelMaker();
    void openAnalogViewer();
    void openImageProcessing();
    void openTongueTracking();
    void openTrackingWidget();
    void openMLWidget();
    void openDataViewer();
    void openPointLoaderWidget();
    void openMaskLoaderWidget();
    void openLineLoaderWidget();
    void openIntervalLoaderWidget();
    void openTensorLoaderWidget();
    void openDataManager();
    void openVideoExportWidget();
    void openDataTransforms();
};
#endif// MAINWINDOW_HPP
