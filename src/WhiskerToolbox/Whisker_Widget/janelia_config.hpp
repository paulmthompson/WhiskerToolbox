#ifndef JANELIA_CONFIG_HPP
#define JANELIA_CONFIG_HPP


#include <QWidget>

namespace Ui {
class janelia_config;
}

/*

This is our interface to using the Janelia whisker tracker.

*/



class Janelia_Config : public QWidget
{
    Q_OBJECT
public:

    Janelia_Config(QWidget *parent = 0);

    virtual ~Janelia_Config();

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:

    Ui::janelia_config *ui;


private slots:

};


#endif // JANELIA_CONFIG_HPP
