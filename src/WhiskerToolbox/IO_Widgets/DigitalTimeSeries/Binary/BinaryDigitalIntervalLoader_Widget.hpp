#ifndef BINARY_DIGITAL_INTERVAL_LOADER_WIDGET_HPP
#define BINARY_DIGITAL_INTERVAL_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "DataManager/DigitalTimeSeries/IO/Binary/Digital_Interval_Series_Binary.hpp"

namespace Ui {
class BinaryDigitalIntervalLoader_Widget;
}

class BinaryDigitalIntervalLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit BinaryDigitalIntervalLoader_Widget(QWidget *parent = nullptr);
    ~BinaryDigitalIntervalLoader_Widget() override;

signals:
    void loadBinaryIntervalRequested(BinaryIntervalLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onDataTypeChanged(int index);

private:
    Ui::BinaryDigitalIntervalLoader_Widget *ui;
    void _updateChannelRange();
};

#endif // BINARY_DIGITAL_INTERVAL_LOADER_WIDGET_HPP 