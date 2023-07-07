#ifndef LABEL_WIDGET_H
#define LABEL_WIDGET_H

#include <QWidget>

#include <memory>

#include "Media_Window.h"
#include "label_maker.h"
#include "ui_Label_Widget.h"

#include <memory>

class Label_Widget : public QWidget, private Ui::Label_Widget
{
    Q_OBJECT
public:
   Label_Widget(Media_Window* scene, QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);

        this->scene = scene;


        this->label_maker = std::make_unique<LabelMaker>();

    };

   void openWidget(); // Call

protected:
   void closeEvent(QCloseEvent *event);
   void keyPressEvent(QKeyEvent *event);


private:
    Media_Window * scene;
    std::unique_ptr<LabelMaker> label_maker;
    void updateAll();
    void updateTable();
    void updateDraw();
    void addLabeltoTable(int row, int frame, label_point label);
private slots:
    void ClickedInVideo(qreal x,qreal y);
    void saveButton();

};

#endif // LABEL_WIDGET_H
