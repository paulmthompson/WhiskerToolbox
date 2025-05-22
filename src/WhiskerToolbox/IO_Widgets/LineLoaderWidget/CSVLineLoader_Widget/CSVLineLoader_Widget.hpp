#ifndef CSVLINELOADER_WIDGET_HPP
#define CSVLINELOADER_WIDGET_HPP

#include <QString>
#include <QWidget>


namespace Ui {
class CSVLineLoader_Widget;
}


class CSVLineLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVLineLoader_Widget(QWidget * parent = nullptr);
    ~CSVLineLoader_Widget() override;


signals:

private:
    Ui::CSVLineLoader_Widget * ui;

private slots:

};


#endif// CSVLINELOADER_WIDGET_HPP
