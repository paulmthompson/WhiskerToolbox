#ifndef RULER_CORNER_WIDGET_HPP
#define RULER_CORNER_WIDGET_HPP

/**
 * @file RulerCornerWidget.hpp
 * @brief Top-left corner square for the Media Viewer ruler layout
 */

#include <QWidget>

class QPaintEvent;

/**
 * @brief Small corner widget where horizontal and vertical rulers meet
 */
class RulerCornerWidget : public QWidget {
    Q_OBJECT

public:
    explicit RulerCornerWidget(QWidget * parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent * event) override;

private:
    static constexpr int kCornerSize = 20;
};

#endif// RULER_CORNER_WIDGET_HPP
