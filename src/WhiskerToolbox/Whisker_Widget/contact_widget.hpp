#ifndef CONTACT_WIDGET_HPP
#define CONTACT_WIDGET_HPP

#include "DataManager.hpp"

#include <QWidget>

#include <memory>
#include <vector>

namespace Ui {
class contact_widget;
}

/*

This is our interface to using the Janelia whisker tracker.

*/


enum class Contact : int {
    Contact = 1,
    NoContact = 0
};

class Contact_Widget : public QWidget
{
    Q_OBJECT
public:

    Contact_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);

    virtual ~Contact_Widget();

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:
    Ui::contact_widget *ui;
    std::shared_ptr<DataManager> _data_manager;

    std::vector<Contact> _contact;
    int _contact_start;
    bool _contact_epoch;


private slots:
    void _contactButton();
    void _saveContact();
    void _loadContact();
};


#endif // CONTACT_WIDGET_HPP
