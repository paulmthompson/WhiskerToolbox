#ifndef CSVLINELOADER_WIDGET_HPP
#define CSVLINELOADER_WIDGET_HPP

#include <QString>
#include <QWidget>
#include "DataManager/Lines/IO/CSV/Line_Data_CSV.hpp"

namespace Ui {
class CSVLineLoader_Widget;
}

class CSVLineLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVLineLoader_Widget(QWidget * parent = nullptr);
    ~CSVLineLoader_Widget() override;

signals:
    void loadSingleFileCSVRequested(QString filepath);
    void loadMultiFileCSVRequested(CSVMultiFileLineLoaderOptions options);

private slots:
    void _onLoadModeChanged();
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();

private:
    Ui::CSVLineLoader_Widget * ui;
    void _updateUIForLoadMode();
};

#endif// CSVLINELOADER_WIDGET_HPP
