#ifndef LABEL_WIDGET_HPP
#define LABEL_WIDGET_HPP

#include <QWidget>

#include <filesystem>
#include <memory>

class DataManager;
struct label_point;
class LabelMaker;
class Media_Window;

namespace Ui {
class Label_Widget;
}

class Label_Widget : public QWidget {
  Q_OBJECT
public:
  Label_Widget(Media_Window *scene, std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~Label_Widget() override;

  void openWidget(); // Call

protected:
  void closeEvent(QCloseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  Media_Window *_scene;
    std::shared_ptr<DataManager> _data_manager;
  std::unique_ptr<LabelMaker> _label_maker;
  void _updateAll();
  void _updateTable();
  void _addLabeltoTable(int row, std::string frame_id, label_point label);
  void _exportFrames(std::string saveFileName);
  std::filesystem::path _createImagePath(std::string saveFileName);
  Ui::Label_Widget *ui;
private slots:
  void _ClickedInVideo(qreal x, qreal y);
  void _saveButton();
  void _changeLabelName();
};

#endif // LABEL_WIDGET_HPP
