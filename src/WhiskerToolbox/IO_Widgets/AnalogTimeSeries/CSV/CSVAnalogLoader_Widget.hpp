#ifndef CSVANALOGLOADER_WIDGET_HPP
#define CSVANALOGLOADER_WIDGET_HPP

#include <QWidget>

struct CSVAnalogLoaderOptions;

namespace Ui {
class CSVAnalogLoader_Widget;
}

class CSVAnalogLoader_Widget : public QWidget {
    Q_OBJECT

public:
    explicit CSVAnalogLoader_Widget(QWidget *parent = nullptr);
    ~CSVAnalogLoader_Widget() override;

signals:
    void loadAnalogCSVRequested(CSVAnalogLoaderOptions const & options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onSingleColumnCheckboxToggled(bool checked);

private:
    Ui::CSVAnalogLoader_Widget *ui;
    void _updateColumnControlsState();
};

#endif // CSVANALOGLOADER_WIDGET_HPP 