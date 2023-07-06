
#include "Label_Widget.h"

#include <iostream>


void Label_Widget::openWidget() {
    std::cout << "Label Widget Opened" << std::endl;

    //connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    connect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));

    this->show();

}

void Label_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
    disconnect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));

}

void Label_Widget::ClickedInVideo(qreal x,qreal y) {

    this->label_maker->addLabel(this->scene->getLastLoadedFrame(), static_cast<int>(x), static_cast<int>(y));

    //update table
    this->updateTable();


}

void Label_Widget::updateTable() {
    tableWidget->setRowCount(0);
    int current_row = 0;
    for (auto i : this->label_maker->getLabels()) {
        tableWidget->insertRow(tableWidget->rowCount());
        this->addLabeltoTable(current_row, i.first,i.second);
        current_row++;
    }
}

void Label_Widget::addLabeltoTable(int row, int frame, label_point label) {
    tableWidget->setItem(row,0,new QTableWidgetItem(QString::number(frame)));
    tableWidget->setItem(row,1,new QTableWidgetItem(QString::number(label.x)));
    tableWidget->setItem(row,2,new QTableWidgetItem(QString::number(label.y)));
}
