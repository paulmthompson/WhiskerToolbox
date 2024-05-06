#ifndef JANELIA_CONFIG_HPP
#define JANELIA_CONFIG_HPP


#include <QWidget>
#include "whiskertracker.h"

#include <memory>

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

    Janelia_Config(std::shared_ptr<WhiskerTracker> tracker, QWidget *parent = 0);

    virtual ~Janelia_Config();

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:
    std::shared_ptr<WhiskerTracker> _wt;
    Ui::janelia_config *ui;


private slots:
    void _changeSeedOnGridLatticeSpacing(int value);
    void _changeSeedSizePx(int value);
    void _changeSeedIterations(int value);
    void _changeSeedIterationThres(double value);
    void _changeSeedAccumThres(double value);
    void _changeSeedThres(double value);
};


#endif // JANELIA_CONFIG_HPP
