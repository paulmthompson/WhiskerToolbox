#ifndef DIGITALINTERVALSERIES_WIDGET_HPP
#define DIGITALINTERVALSERIES_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <filesystem>
#include <string>

namespace Ui { class DigitalIntervalSeries_Widget; }

class DataManager;

class DigitalIntervalSeries_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~DigitalIntervalSeries_Widget();

    void openWidget(); // Call to open the widget
    void setOutputPath(std::filesystem::path path) {_output_path = path;};

private:
    Ui::DigitalIntervalSeries_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    std::filesystem::path _output_path;
private slots:
    void _saveCSV();
};

#endif // DIGITALINTERVALSERIES_WIDGET_HPP
