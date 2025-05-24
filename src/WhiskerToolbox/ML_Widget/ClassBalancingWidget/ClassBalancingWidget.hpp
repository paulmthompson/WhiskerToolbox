#ifndef CLASSBALANCINGWIDGET_HPP
#define CLASSBALANCINGWIDGET_HPP

#include <QWidget>

// Forward declaration of the UI class
namespace Ui {
    class ClassBalancingWidget;
}

class ClassBalancingWidget : public QWidget {
    Q_OBJECT

public:
    explicit ClassBalancingWidget(QWidget *parent = nullptr);
    ~ClassBalancingWidget(); // Destructor to clean up UI

    // Getters for the current settings
    bool isBalancingEnabled() const;
    double getBalancingRatio() const;
    
    // Setters
    void setBalancingEnabled(bool enabled);
    void setBalancingRatio(double ratio);
    
    // Update the class distribution display
    void updateClassDistribution(const QString& distributionText);
    void clearClassDistribution();

signals:
    void balancingSettingsChanged();

private slots:
    void onBalancingToggled(bool enabled);
    void onRatioChanged(double ratio);

private:
    Ui::ClassBalancingWidget *ui; // Pointer to the UI class
};

#endif // CLASSBALANCINGWIDGET_HPP 