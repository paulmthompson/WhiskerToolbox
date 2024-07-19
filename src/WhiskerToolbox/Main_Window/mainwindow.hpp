#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "Media_Window.hpp"

#include "TimeFrame.hpp"
#include "DataManager.hpp"

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QTimer>
#include <QPointer>

#include "DockManager.h"

#include <map>
#include <memory>
#include <string>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void addWidget(std::string const & key, std::unique_ptr<QWidget> widget) {
        _widgets[key] = std::move(widget);
    }

    QWidget* getWidget(std::string const & key) {
        auto it = _widgets.find(key);
        if (it != _widgets.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    void registerDockWidget(std::string const & key, QWidget* widget, ads::DockWidgetArea area);
    void showDockWidget(std::string const & key);

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;

    Media_Window* _scene;

    ads::CDockManager* _m_DockManager;

    bool _verbose;

    std::shared_ptr<DataManager> _data_manager;

    std::map<std::string, std::unique_ptr<QWidget>> _widgets;

    void _updateMedia();

    void _createActions();

    void _LoadData(std::string filepath);

private slots:
    void Load_Video();
    void Load_Images();

    void _loadTimeSeriesCSV();

    void openWhiskerTracking();
    void openLabelMaker();
    void openAnalogViewer();
    void openImageProcessing();
    void openTongueTracking();
};
#endif // MAINWINDOW_HPP
