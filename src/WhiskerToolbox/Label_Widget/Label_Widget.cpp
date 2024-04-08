
#include "Label_Widget.h"

#include <fstream>
#include <iostream>
#include <iterator>

#include <QFileDialog>
#include <QKeyEvent>

#include "ui_Label_Widget.h"

Label_Widget::Label_Widget(Media_Window* scene, std::shared_ptr<TimeFrame> time, QWidget *parent) :
    QWidget(parent),  ui(new Ui::Label_Widget)
{
    ui->setupUi(this);

    _scene = scene;
    _time = time;

    _label_maker = std::make_unique<LabelMaker>();
}

Label_Widget::~Label_Widget() {
    delete ui;
}

void Label_Widget::openWidget() {
  std::cout << "Label Widget Opened" << std::endl;

  // connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
  connect(_scene, SIGNAL(leftClick(qreal, qreal)), this,
          SLOT(_ClickedInVideo(qreal, qreal)));
  connect(ui->saveLabelsButton, SIGNAL(clicked()), this, SLOT(_saveButton()));

  connect(ui->label_name_box, SIGNAL(textChanged()), this,
          SLOT(_changeLabelName()));

  this->show();
}

void Label_Widget::closeEvent(QCloseEvent *event) {
  std::cout << "Close event detected" << std::endl;
  disconnect(_scene, SIGNAL(leftClick(qreal, qreal)), this,
             SLOT(_ClickedInVideo(qreal, qreal)));
  disconnect(ui->saveLabelsButton, SIGNAL(clicked()), this,
             SLOT(_saveButton()));
}

void Label_Widget::keyPressEvent(QKeyEvent *event) {

  if (event->key() == Qt::Key_Delete) {
    std::cout << "Delete key pressed" << std::endl;
      if (ui->tableWidget->selectedItems().empty()) {
      std::cout << "No items in the table are selected" << std::endl;
    } else {
          int selected_row_number = ui->tableWidget->selectedItems().first()->row();
      std::cout << "Row selected is " << selected_row_number << std::endl;

      auto selected_frame =
          ui->tableWidget->item(selected_row_number, 0)->text().toStdString();
      std::cout << "Corresponding selected frame is " << selected_frame
                << std::endl;

      _label_maker->removeLabel(selected_frame);
      this->_updateAll();
    }
  }
}

void Label_Widget::_ClickedInVideo(qreal x_canvas, qreal y_canvas) {

  float x_media = x_canvas / _scene->getXAspect();
  float y_media = y_canvas / _scene->getYAspect();

  // Generate the image to be labeled
  int frame_number = _time->getLastLoadedFrame();
  std::string frame_id = _scene->getFrameID(frame_number);
  auto img = _label_maker->createImage(_scene->getMediaHeight(),
                                       _scene->getMediaWidth(), frame_number,
                                       frame_id, _scene->getCurrentFrame());

  _label_maker->addLabel(img, static_cast<int>(x_media),
                         static_cast<int>(y_media));

  this->_updateAll();
}

void Label_Widget::_updateAll() {
  _updateDraw();
  _updateTable();
}

// If current frame has label, it should be redrawn

void Label_Widget::_updateDraw() {
  _scene->clearPoints();
  for (auto &[frame_name, label] : _label_maker->getLabels()) {
    if (frame_name == _scene->getFrameID(_time->getLastLoadedFrame())) {
      auto &[img, point] = label;
      _scene->addPoint(point.x, point.y, QPen(QColor(Qt::red)));
    }
  }
}

void Label_Widget::_updateTable() {

  // The table is erased and rebuilt from scratch
    ui->tableWidget->setRowCount(0);
  int current_row = 0;
  for (auto &[frame_name, label] : _label_maker->getLabels()) {
    auto &[img, point] = label;
    this->_addLabeltoTable(current_row, frame_name, point);
    current_row++;
  }
}

void Label_Widget::_addLabeltoTable(int row, std::string frame_id,
                                    label_point label) {
    ui->tableWidget->insertRow(ui->tableWidget->rowCount());
    ui->tableWidget->setItem(row, 0,
                       new QTableWidgetItem(QString::fromStdString(frame_id)));
    ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(label.x)));
    ui->tableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(label.y)));
}

void Label_Widget::_saveButton() {

  auto output_stream = _label_maker->saveLabelsJSON();
  // std::cout << output_stream.str() << std::endl;
  QString saveFileName = QFileDialog::getSaveFileName(this, tr("Save File"), "",
                                                      tr("JSON (*.json)"));

  std::ofstream outFile;
  outFile.open(saveFileName.toStdString());

  outFile << output_stream.str() << std::endl;

  outFile.close();

  if (ui->export_frames_checkbox->isChecked()) {
    std::cout << "Exporting frames" << std::endl;
    _exportFrames(saveFileName.toStdString());
  }
}

void Label_Widget::_changeLabelName() {
    _label_maker->changeLabelName(ui->label_name_box->toPlainText().toStdString());
}

void Label_Widget::_exportFrames(std::string saveFileName) {

  std::filesystem::path saveFilePath = _createImagePath(saveFileName);

  for (auto &[frame_name, label] : _label_maker->getLabels()) {
    auto &[img, point] = label;

    QImage labeled_image(&img.data[0], img.width, img.height,
                         QImage::Format_Grayscale8);

    std::string saveName = saveFilePath.string() + "/" + img.frame_id + ".png";
    std::cout << "Saving file" << saveName << std::endl;

    labeled_image.save(QString::fromStdString(saveName));
  }
}

std::filesystem::path Label_Widget::_createImagePath(std::string saveFileName) {

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
