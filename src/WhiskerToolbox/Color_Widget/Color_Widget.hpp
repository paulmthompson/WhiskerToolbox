#ifndef COLOR_WIDGET_HPP
#define COLOR_WIDGET_HPP

#include <QHBoxLayout>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QWidget>

class ColorWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColorWidget(QWidget * parent = nullptr)
        : QWidget(parent) {
        _lineEdit = new QLineEdit(this);

        auto layout = new QHBoxLayout(this);
        layout->addWidget(_lineEdit);
        layout->setContentsMargins(0, 0, 0, 0);

        QRegularExpression const hexRegExp("^#[0-9A-Fa-f]{6}$");
        auto validator = new QRegularExpressionValidator(hexRegExp, this);
        _lineEdit->setValidator(validator);

        connect(_lineEdit, &QLineEdit::textChanged, this, &ColorWidget::updateColor);
    }

    [[nodiscard]] QString text() const {
        return _lineEdit->text();
    }

    void setText(QString const & text) {
        _lineEdit->setText(text);
        updateColor(text);
    }

signals:
    void colorChanged(QString const & color);

private slots:
    void updateColor(QString const & color) {
        QPalette palette = _lineEdit->palette();
        palette.setColor(QPalette::Window, QColor::fromString(color));
        _lineEdit->setAutoFillBackground(true);
        _lineEdit->setPalette(palette);
        _lineEdit->update();
        emit colorChanged(color);
    }

private:
    QLineEdit * _lineEdit;
};

#endif// COLOR_WIDGET_HPP
