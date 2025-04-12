#ifndef JANELIA_CONFIG_HPP
#define JANELIA_CONFIG_HPP

#include <QWidget>

#include <memory>

namespace whisker {class WhiskerTracker;}

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

    explicit Janelia_Config(std::shared_ptr<whisker::WhiskerTracker> tracker, QWidget *parent = nullptr);

    ~Janelia_Config() override;

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event) override;

private:
    std::shared_ptr<whisker::WhiskerTracker> _wt;
    Ui::janelia_config *ui;


private slots:
    void _changeSeedOnGridLatticeSpacing(int value);
    void _changeSeedSizePx(int value);
    void _changeSeedIterations(int value);
    void _changeSeedIterationThres(double value);
    void _changeSeedAccumThres(double value);
    void _changeSeedThres(double value);
    void _changeTLen(int value);
    void _changeOffsetStep(double value);
    void _changeAngleStep(double value);

    void _changeWidthStep(double value);
    void _changeWidthMin(double value);
    void _changeWidthMax(double value);

    void _changeMinSignal(double value);

    void _changeMaxDeltaAngle(double value);
    void _changeMaxDeltaWidth(double value);
    void _changeMaxDeltaOffset(double value);

    void _changeHalfSpaceAsymmetryThreshold(double value);
    void _changeHalfSpaceTunnelingMaxMoves(int value);
};


#endif // JANELIA_CONFIG_HPP
