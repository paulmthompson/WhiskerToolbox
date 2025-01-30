#ifndef COLOR_WIDGET_HPP
#define COLOR_WIDGET_HPP

#include <QWidget>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QRegularExpressionValidator>

class ColorWidget : public QWidget {
    Q_OBJECT

public:
    ColorWidget(QWidget *parent = nullptr) : QWidget(parent) {
        _lineEdit = new QLineEdit(this);

        auto layout = new QHBoxLayout(this);
        layout->addWidget(_lineEdit);
        layout->setContentsMargins(0, 0, 0, 0);

        QRegularExpression hexRegExp("^#[0-9A-Fa-f]{6}$");
        auto validator = new QRegularExpressionValidator(hexRegExp, this);
        _lineEdit->setValidator(validator);

        connect(_lineEdit, &QLineEdit::textChanged, this, &ColorWidget::updateColor);
    }

    QString text() const {
        return _lineEdit->text();
    }

    void setText(const QString &text) {
        _lineEdit->setText(text);
        updateColor(text);
    }

signals:
    void colorChanged(const QString &color);

private slots:
    void updateColor(const QString &color) {
        QPalette palette = _lineEdit->palette();
        palette.setColor(QPalette::Window, QColor::fromString(color));
        _lineEdit->setAutoFillBackground(true);
        _lineEdit->setPalette(palette);
        _lineEdit->update();
        emit colorChanged(color);
    }

private:
    QLineEdit *_lineEdit;
};

#endif // COLOR_WIDGET_HPP
