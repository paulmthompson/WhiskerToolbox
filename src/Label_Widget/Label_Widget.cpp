
#include "Label_Widget.h"

#include <iostream>
#include <iterator>
#include <fstream>

#include <QKeyEvent>
#include <QFileDialog>

void Label_Widget::openWidget() {
    std::cout << "Label Widget Opened" << std::endl;

    //connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    connect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));
    connect(this->saveLabelsButton,SIGNAL(clicked()),this,SLOT(saveButton()));

    connect(this->label_name_box,SIGNAL(textChanged()),this,SLOT(changeLabelName()));

    this->show();

}

void Label_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
    disconnect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));
    disconnect(this->saveLabelsButton,SIGNAL(clicked()),this,SLOT(saveButton()));

}

void Label_Widget::keyPressEvent(QKeyEvent *event) {

    if (event->key() == Qt::Key_Delete) {
        std::cout << "Delete key pressed" << std::endl;
        if (tableWidget->selectedItems().empty()) {
            std::cout << "No items in the table are selected" << std::endl;
        } else {
            int selected_row_number = tableWidget->selectedItems().first()->row();
            std::cout << "Row selected is " << selected_row_number << std::endl;

            auto selected_frame = tableWidget->item(selected_row_number,0)->text().toStdString();
            std::cout << "Corresponding selected frame is " << selected_frame << std::endl;

            this->label_maker->removeLabel(selected_frame);
            this->updateAll();
        }
    }

}

void Label_Widget::ClickedInVideo(qreal x_canvas,qreal y_canvas) {

    float x_media = x_canvas / this->scene->getXAspect();
    float y_media = y_canvas / this->scene->getYAspect();

    //Generate the image to be labeled
    int frame_number = this->scene->getLastLoadedFrame();
    std::string frame_id = this->scene->getFrameID(frame_number);
    auto img = label_maker->createImage(this->scene->getMediaHeight(),this->scene->getMediaWidth(),frame_number,frame_id, this->scene->getCurrentFrame());

    this->label_maker->addLabel(img, static_cast<int>(x_media), static_cast<int>(y_media));

    this->updateAll();
}

void Label_Widget::updateAll() {
    updateDraw();
    updateTable();
}

//If current frame has label, it should be redrawn

void Label_Widget::updateDraw() {
    scene->clearPoints();
    for (auto& [frame_name,label] : this->label_maker->getLabels()) {
        if (frame_name == scene->getFrameID(scene->getLastLoadedFrame())) {
            auto& [img, point] = label;
            this->scene->addPoint(point.x,point.y,QPen(QColor(Qt::red)));
        }
    }

}

void Label_Widget::updateTable() {

    //The table is erased and rebuilt from scratch
    tableWidget->setRowCount(0);
    int current_row = 0;
    for (auto& [frame_name, label] : this->label_maker->getLabels()) {
        auto& [img, point] = label;
        this->addLabeltoTable(current_row, frame_name,point);
        current_row++;
    }
}

void Label_Widget::addLabeltoTable(int row, std::string frame_id, label_point label) {
    tableWidget->insertRow(tableWidget->rowCount());
    tableWidget->setItem(row,0,new QTableWidgetItem(QString::fromStdString(frame_id)));
    tableWidget->setItem(row,1,new QTableWidgetItem(QString::number(label.x)));
    tableWidget->setItem(row,2,new QTableWidgetItem(QString::number(label.y)));
}

void Label_Widget::saveButton() {

    auto output_stream = this->label_maker->saveLabelsJSON();
    //std::cout << output_stream.str() << std::endl;
    QString saveFileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                    "",
                                                    tr("JSON (*.json)"));

    std::ofstream outFile;
    outFile.open(saveFileName.toStdString());

    outFile << output_stream.str() << std::endl;

    outFile.close();

    if (export_frames_checkbox->isChecked()) {
        std::cout << "Exporting frames" << std::endl;
        exportFrames(saveFileName.toStdString());
    }

}

void Label_Widget::changeLabelName() {
    this->label_maker->changeLabelName(label_name_box->toPlainText().toStdString());
}

void Label_Widget::exportFrames(std::string saveFileName) {

    std::filesystem::path saveFilePath = createImagePath(saveFileName);

    for (auto& [frame_name,label] : this->label_maker->getLabels()) {
        auto& [img, point] = label;

        QImage labeled_image(&img.data[0],img.width,img.height, QImage::Format_Grayscale8);

        std::string saveName = saveFilePath.string() + "/" + img.frame_id + ".png";
        std::cout << "Saving file" << saveName << std::endl;

        labeled_image.save(QString::fromStdString(saveName));
    }
}

std::filesystem::path Label_Widget::createImagePath(std::string saveFileName) {

    std::filesystem::path saveFilePath = saveFileName;

    saveFilePath = saveFilePath.parent_path() / "images";

    if (!std::filesystem::exists(saveFilePath)) {
        std::filesystem::create_directory(saveFilePath);
        std::cout << "Creating directory at " << saveFilePath.string() << std::endl;
    } else {
        std::cout << "Images directory already exists" << std::endl;
    }

    return saveFilePath;
}
