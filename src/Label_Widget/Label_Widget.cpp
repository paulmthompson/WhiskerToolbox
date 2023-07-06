
#include "Label_Widget.h"

#include <iostream>
#include <iterator>

#include <QKeyEvent>


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

void Label_Widget::keyPressEvent(QKeyEvent *event) {

    if (event->key() == Qt::Key_Delete) {
        std::cout << "Delete key pressed" << std::endl;
        if (tableWidget->selectedItems().empty()) {
            std::cout << "No items in the table are selected" << std::endl;
        } else {
            int selected_row_number = tableWidget->selectedItems().first()->row();
            std::cout << "Row selected is " << selected_row_number << std::endl;

            int selected_frame = tableWidget->item(selected_row_number,0)->text().toInt();
            std::cout << "Corresponding selected frame is " << selected_frame << std::endl;

            this->label_maker->removeLabel(selected_frame);
            this->updateAll();
        }
    }

}

void Label_Widget::ClickedInVideo(qreal x,qreal y) {

    this->label_maker->addLabel(this->scene->getLastLoadedFrame(), static_cast<int>(x), static_cast<int>(y));

    this->updateAll();
}

void Label_Widget::updateAll() {
    updateDraw();
    updateTable();
}

//If current frame has label, it should be redrawn

void Label_Widget::updateDraw() {
    scene->clearPoints();
    for (auto i : this->label_maker->getLabels()) {
        if (i.first == scene->getLastLoadedFrame()) {
            this->scene->addPoint(i.second.x,i.second.y,QPen(QColor(Qt::red)));
        }
    }

}

void Label_Widget::updateTable() {

    //The table is erased and rebuilt from scratch
    tableWidget->setRowCount(0);
    int current_row = 0;
    for (auto i : this->label_maker->getLabels()) {
        this->addLabeltoTable(current_row, i.first,i.second);
        current_row++;
    }
}

void Label_Widget::addLabeltoTable(int row, int frame, label_point label) {
    tableWidget->insertRow(tableWidget->rowCount());
    tableWidget->setItem(row,0,new QTableWidgetItem(QString::number(frame)));
    tableWidget->setItem(row,1,new QTableWidgetItem(QString::number(label.x)));
    tableWidget->setItem(row,2,new QTableWidgetItem(QString::number(label.y)));
}
