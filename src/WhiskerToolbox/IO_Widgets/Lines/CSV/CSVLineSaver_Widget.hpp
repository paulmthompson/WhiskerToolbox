#ifndef CSV_LINE_SAVER_WIDGET_HPP
#define CSV_LINE_SAVER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "nlohmann/json.hpp"

// Forward declaration
namespace Ui {
class CSVLineSaver_Widget;
}

class CSVLineSaver_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVLineSaver_Widget(QWidget *parent = nullptr);
    ~CSVLineSaver_Widget() override;

signals:
    void saveCSVRequested(QString format, nlohmann::json config);
    void saveMultiFileCSVRequested(QString format, nlohmann::json config);

private slots:
    void _onSaveHeaderCheckboxToggled(bool checked);
    void _updatePrecisionExample(int precision);
    void _onSaveModeChanged();

private:
    Ui::CSVLineSaver_Widget *ui;
    void _updatePrecisionLabelText(int precision);
    void _updateUIForSaveMode();
};

#endif // CSV_LINE_SAVER_WIDGET_HPP 