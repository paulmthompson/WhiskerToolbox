#ifndef COLORPICKER_WIDGET_HPP
#define COLORPICKER_WIDGET_HPP

#include <QColor>
#include <QWidget>

#include <memory>

namespace Ui {
class ColorPicker_Widget;
}

class ColorPicker_Widget : public QWidget {
    Q_OBJECT

public:
    explicit ColorPicker_Widget(QWidget* parent = nullptr);
    ~ColorPicker_Widget();

    void setColor(const QString& hex_color);
    void setColor(int r, int g, int b);
    void setAlpha(int alpha_percent);

    QString getHexColor() const;
    QColor getColor() const;
    int getAlphaPercent() const;
    float getAlphaFloat() const;

signals:
    void colorChanged(const QString& hex_color);
    void alphaChanged(int alpha_percent);
    void colorAndAlphaChanged(const QString& hex_color, float alpha);

private slots:
    void _onRgbChanged();
    void _onHexChanged();
    void _onAlphaChanged(int value);
    void _updateColorDisplay();

private:
    Ui::ColorPicker_Widget* ui;
    bool _updating = false;
};


#endif// COLORPICKER_WIDGET_HPP
