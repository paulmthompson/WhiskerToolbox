#ifndef CONTACT_WIDGET_HPP
#define CONTACT_WIDGET_HPP

#include "DataManager.hpp"

#include <QWidget>
#include <QGraphicsScene>
#include <QImage>

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

struct ContactEvent {
    int start;
    int end;
};

class Contact_Widget : public QWidget
{
    Q_OBJECT
public:

    Contact_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);

    virtual ~Contact_Widget();

    void openWidget(); // Call

    void updateFrame(int frame_id);
    void setPolePos(float pole_x, float pole_y);
protected:
    void closeEvent(QCloseEvent *event);

private:
    Ui::contact_widget *ui;
    std::shared_ptr<DataManager> _data_manager;

    std::vector<Contact> _contact;
    int _contact_start;
    bool _contact_epoch;
    std::vector<ContactEvent> _contactEvents;
    QGraphicsScene* _scene;
    std::vector<QImage> _contact_imgs;
    int _image_buffer_size;
    std::tuple<int, int> _pole_pos;
    bool _pole_select_mode;

    void _buildContactTable();
    void _calculateContactPeriods();
    QImage::Format _getQImageFormat();

private slots:
    void _contactButton();
    void _saveContact();
    void _loadContact();
    void _poleSelectButton();
};


#endif // CONTACT_WIDGET_HPP
