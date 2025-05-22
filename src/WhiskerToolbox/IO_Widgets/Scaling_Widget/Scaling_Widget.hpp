#ifndef SCALING_WIDGET_HPP
#define SCALING_WIDGET_HPP

#include "../../DataManager/ImageSize/ImageSize.hpp"

#include <QWidget>

namespace Ui {
class Scaling_Widget;
}

class Scaling_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Scaling_Widget(QWidget * parent = nullptr);
    ~Scaling_Widget() override;

    ImageSize getOriginalImageSize() const;
    ImageSize getScaledImageSize() const;
    bool isScalingEnabled() const;

private slots:
    void _enableImageScaling(bool enable);

signals:
    void scalingParametersChanged();

private:
    Ui::Scaling_Widget * ui;
};

#endif // SCALING_WIDGET_HPP
