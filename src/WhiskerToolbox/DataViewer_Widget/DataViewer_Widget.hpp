
#ifndef DATAVIEWER_WIDGET_HPP
#define DATAVIEWER_WIDGET_HPP

#include <QMainWindow>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class DataManager;
class MainWindow;
class QTableWidget;
class TimeScrollBar;
class TimeFrame;
class Feature_Table_Widget;
class QWheelEvent;

namespace Ui { class DataViewer_Widget; }

class DataViewer_Widget : public QMainWindow {
    Q_OBJECT

public:
    DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                      TimeScrollBar *time_scrollbar,
                      MainWindow *main_window,
                      QWidget *parent = 0);

    virtual ~DataViewer_Widget();

    void openWidget();

    void updateXAxisSamples(int value);

protected:
    void closeEvent(QCloseEvent *event);
    void wheelEvent(QWheelEvent *event) override;
private slots:
    //void _insertRows(const std::vector<std::string>& keys);
    void _addFeatureToModel(const QString& feature, bool enabled);
    //void _highlightAvailableFeature(int row, int column);
    void _plotSelectedFeature(const std::string key);
    void _removeSelectedFeature(const std::string key);
    void _updatePlot(int time);
    void _handleFeatureSelected(const QString& feature);
    void _handleXAxisSamplesChanged(int value);
private:

    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    MainWindow* _main_window;
    Ui::DataViewer_Widget *ui;

    std::shared_ptr<TimeFrame> _time_frame;

    QString _highlighted_available_feature;

    void _updateLabels();

};


#endif //DATAVIEWER_WIDGET_HPP
