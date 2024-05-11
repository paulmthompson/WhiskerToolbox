#ifndef TIME_SCROLLBAR_H
#define TIME_SCROLLBAR_H

#include "DataManager.hpp"

#include <QWidget>

#include <memory>
#include <vector>

namespace Ui {
class TimeScrollBar;
}

class TimeScrollBar : public QWidget
{
    Q_OBJECT
public:

    TimeScrollBar(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);

    virtual ~TimeScrollBar();

protected:
private:
    Ui::TimeScrollBar *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:

};


#endif // TIME_SCROLLBAR_H
