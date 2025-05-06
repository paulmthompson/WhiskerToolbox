#ifndef LMDBLINELOADER_WIDGET_HPP
#define LMDBLINELOADER_WIDGET_HPP

#include <QString>
#include <QWidget>


namespace Ui {
class LMDBLineLoader_Widget;
}


class LMDBLineLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit LMDBLineLoader_Widget(QWidget * parent = nullptr);
    ~LMDBLineLoader_Widget() override;


signals:

private:
    Ui::LMDBLineLoader_Widget * ui;

private slots:

};


#endif// LMDBLINELOADER_WIDGET_HPP
