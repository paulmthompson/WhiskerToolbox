#ifndef ANALYSIS_DASHBOARD_HPP
#define ANALYSIS_DASHBOARD_HPP

#include <QMainWindow>


namespace Ui {
class Analysis_Dashboard;
}

class Analysis_Dashboard : public QMainWindow {
    Q_OBJECT

public:
    explicit Analysis_Dashboard(QWidget* parent = nullptr);
    ~Analysis_Dashboard();

    void openWidget();

signals:

private slots:


private:
    Ui::Analysis_Dashboard* ui;
};


#endif// ANALYSIS_DASHBOARD_HPP
