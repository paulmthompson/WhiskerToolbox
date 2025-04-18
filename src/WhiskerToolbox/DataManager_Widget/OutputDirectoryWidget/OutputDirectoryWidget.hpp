#ifndef DATAMANAGER_OUTPUTDIRECTORYWIDGET_HPP
#define DATAMANAGER_OUTPUTDIRECTORYWIDGET_HPP

#include <QString>
#include <QWidget>


namespace Ui {
class OutputDirectoryWidget;
}


class OutputDirectoryWidget : public QWidget {
    Q_OBJECT
public:
    explicit OutputDirectoryWidget(QWidget * parent = nullptr);
    ~OutputDirectoryWidget() override;

    void setDirLabel(QString const label);

signals:
    void dirChanged(QString newDir);

private:
    Ui::OutputDirectoryWidget * ui;

private slots:
    void _changeOutputDir();
};

#endif// DATAMANAGER_OUTPUTDIRECTORYWIDGET_HPP
