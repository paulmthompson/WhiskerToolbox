#ifndef BEHAVIORTOOLBOX_ML_WIDGET_HPP
#define BEHAVIORTOOLBOX_ML_WIDGET_HPP

#include <QMainWindow>

#include <mlpack/core.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class DataManager;
class DigitalIntervalSeries;
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
    void _handleFeatureSelected(const QString& feature);
    void _addFeatureToModel(const QString& feature, bool enabled);
    void _removeSelectedFeature(const std::string key);

    void _handleMaskSelected(const QString& feature);
    void _addMaskToModel(const QString& feature, bool enabled);
    void _removeSelectedMask(const std::string key);

    void _handleOutcomeSelected(const QString& feature);
    void _addOutcomeToModel(const QString& feature, bool enabled);
    void _removeSelectedOutcome(const std::string key);

    void _selectModelType(const QString& model_type);

    void _fitModel();
private:
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    MainWindow* _main_window;
    Ui::ML_Widget *ui;
    QString _highlighted_available_feature;

    std::unordered_set<std::string> _selected_features;
    std::unordered_set<std::string> _selected_masks;
    std::unordered_set<std::string> _selected_outcomes;

    arma::Mat<double> _features;
    arma::Mat<double> _outcomes;

};

arma::Mat<double> create_arrays(std::unordered_set<std::string> features,
                                std::vector<std::size_t>& timestamps,
                                std::shared_ptr<DataManager> data_manager);

std::vector<std::size_t> create_timestamps(std::shared_ptr<DigitalIntervalSeries>& series);

#endif //BEHAVIORTOOLBOX_ML_WIDGET_HPP
