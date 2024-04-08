#ifndef LABEL_WIDGET_H
#define LABEL_WIDGET_H

#include <QWidget>

#include <filesystem>
#include <memory>

#include "Media_Window.h"
#include "label_maker.h"

#include "TimeFrame.h"

namespace Ui {
class Label_Widget;
}

class Label_Widget : public QWidget {
  Q_OBJECT
public:
  Label_Widget(Media_Window *scene, std::shared_ptr<TimeFrame> time, QWidget *parent = 0);
    ~Label_Widget();

  void openWidget(); // Call

protected:
  void closeEvent(QCloseEvent *event);
  void keyPressEvent(QKeyEvent *event);

private:
  Media_Window *_scene;
  std::unique_ptr<LabelMaker> _label_maker;
  std::shared_ptr<TimeFrame> _time;
  void _updateAll();
  void _updateTable();
  void _updateDraw();
  void _addLabeltoTable(int row, std::string frame_id, label_point label);
  void _exportFrames(std::string saveFileName);
  std::filesystem::path _createImagePath(std::string saveFileName);
  Ui::Label_Widget *ui;
private slots:
  void _ClickedInVideo(qreal x, qreal y);
  void _saveButton();
  void _changeLabelName();
};

#endif // LABEL_WIDGET_H
