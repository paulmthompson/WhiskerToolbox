
#ifndef DATAVIEWER_WIDGET_HPP
#define DATAVIEWER_WIDGET_HPP

#include <QWidget>

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

namespace Ui {
class DataViewer_Widget;
}

class DataViewer_Widget : public QWidget {
    Q_OBJECT

public:
    DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                      TimeScrollBar * time_scrollbar,
                      MainWindow * main_window,
                      QWidget * parent = nullptr);

    ~DataViewer_Widget() override;

    void openWidget();

    void updateXAxisSamples(int value);

protected:
    void closeEvent(QCloseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
private slots:
    //void _insertRows(const std::vector<std::string>& keys);
    void _addFeatureToModel(QString const & feature, bool enabled);
    //void _highlightAvailableFeature(int row, int column);
    void _plotSelectedFeature(std::string const & key);
    void _removeSelectedFeature(std::string const & key);
    void _updatePlot(int time);
    void _handleFeatureSelected(QString const & feature);
    void _handleXAxisSamplesChanged(int value);
    void _updateGlobalScale(double scale);

private:
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar * _time_scrollbar;
    MainWindow * _main_window;
    Ui::DataViewer_Widget * ui;

    std::shared_ptr<TimeFrame> _time_frame;

    QString _highlighted_available_feature;

    void _updateLabels();
};


#endif//DATAVIEWER_WIDGET_HPP
