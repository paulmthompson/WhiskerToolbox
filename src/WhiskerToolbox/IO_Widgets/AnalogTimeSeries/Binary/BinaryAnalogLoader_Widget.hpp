#ifndef BINARYANALOGLOADER_WIDGET_HPP
#define BINARYANALOGLOADER_WIDGET_HPP

#include <QWidget>

struct BinaryAnalogLoaderOptions;

namespace Ui {
class BinaryAnalogLoader_Widget;
}

class BinaryAnalogLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit BinaryAnalogLoader_Widget(QWidget * parent = nullptr);
    ~BinaryAnalogLoader_Widget() override;

signals:
    void loadBinaryAnalogRequested(BinaryAnalogLoaderOptions const & options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();

private:
    Ui::BinaryAnalogLoader_Widget * ui;
};

#endif// BINARYANALOGLOADER_WIDGET_HPP