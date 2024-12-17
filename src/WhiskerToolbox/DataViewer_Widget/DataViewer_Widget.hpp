
#ifndef DATAVIEWER_WIDGET_HPP
#define DATAVIEWER_WIDGET_HPP

#include <QMainWindow>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class DataManager;
class MainWindow;
class Media_Window;
class QTableWidget;
class TimeScrollBar;

namespace Ui { class DataViewer_Widget; }

class DataViewer_Widget : public QMainWindow {
    Q_OBJECT

public:
    DataViewer_Widget(Media_Window *scene,
                      std::shared_ptr<DataManager> data_manager,
                      TimeScrollBar *time_scrollbar,
                      MainWindow *main_window,
                      QWidget *parent = 0);

    virtual ~DataViewer_Widget();

    void openWidget();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void _refreshAvailableFeatures();
    void _insertRows(const std::vector<std::string>& keys);
    void _addFeatureToModel();
    void _highlightAvailableFeature(int row, int column);
    void _highlightModelFeature(int row, int column);
    void _deleteFeatureFromModel();
    void _plotSelectedFeature();
private:
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    MainWindow* _main_window;
    Ui::DataViewer_Widget *ui;

    QString _highlighted_available_feature;
    QString _highlighted_model_feature;
    std::unordered_set<std::string> _model_features;

};


#endif //DATAVIEWER_WIDGET_HPP
