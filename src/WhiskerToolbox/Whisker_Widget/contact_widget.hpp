#ifndef CONTACT_WIDGET_HPP
#define CONTACT_WIDGET_HPP

#include <QWidget>

#include <memory>

namespace Ui {
class contact_widget;
}

/*

This is our interface to using the Janelia whisker tracker.

*/



class Contact_Widget : public QWidget
{
    Q_OBJECT
public:

    Contact_Widget(QWidget *parent = 0);

    virtual ~Contact_Widget();

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:
    Ui::contact_widget *ui;


private slots:

};


#endif // CONTACT_WIDGET_HPP
