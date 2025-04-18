#ifndef NEWDATAWIDGET_HPP
#define NEWDATAWIDGET_HPP

#include <QString>
#include <QWidget>

#include <string>


namespace Ui {
class NewDataWidget;
}


class NewDataWidget : public QWidget {
    Q_OBJECT
public:
    explicit NewDataWidget(QWidget * parent = nullptr);
    ~NewDataWidget() override;

    //void setDirLabel(QString const label);

signals:
    void createNewData(std::string key, std::string type);

private:
    Ui::NewDataWidget * ui;

private slots:
    void _createNewData();
};


#endif// NEWDATAWIDGET_HPP
