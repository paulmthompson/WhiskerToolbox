#ifndef BEHAVIORTOOLBOX_ML_WIDGET_HPP
#define BEHAVIORTOOLBOX_ML_WIDGET_HPP

#include <QMainWindow>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class DataManager;
class MainWindow;
class QTableWidget;
class TimeScrollBar;

namespace Ui { class ML_Widget; }

class ML_Widget : public QMainWindow {
Q_OBJECT

public:
    ML_Widget(std::shared_ptr<DataManager> data_manager,
              TimeScrollBar *time_scrollbar,
              MainWindow *main_window,
              QWidget *parent = 0);

    virtual ~ML_Widget();

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
    void _addLabelToModel();
    void _deleteLabel();

private:
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    MainWindow* _main_window;
    Ui::ML_Widget *ui;

    QString _highlighted_available_feature;
    QString _highlighted_model_feature;
    std::unordered_set<std::string> _model_features;
    QString _selected_label;
};


#endif //BEHAVIORTOOLBOX_ML_WIDGET_HPP
