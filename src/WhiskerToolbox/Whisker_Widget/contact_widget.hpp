#ifndef CONTACT_WIDGET_HPP
#define CONTACT_WIDGET_HPP

#include <QImage>
#include <QWidget>

#include <memory>
#include <filesystem>
#include <vector>

class DataManager;
class QGraphicsPathItem;
class QGraphicsPixmapItem;
class QGraphicsScene;
class QTableWidget;
class TimeScrollBar;

namespace Ui {
class contact_widget;
}


class Contact_Widget : public QWidget
{
    Q_OBJECT
public:

    Contact_Widget(std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent = 0);

    virtual ~Contact_Widget();

    void openWidget(); // Call

    void updateFrame(int frame_id);
    void setPolePos(float pole_x, float pole_y);
protected:
    void closeEvent(QCloseEvent *event);

private:
    Ui::contact_widget *ui;
    std::shared_ptr<DataManager> _data_manager;

    int _contact_start {0};
    bool _contact_epoch {false};
    QGraphicsScene* _scene;
    std::vector<QImage> _contact_imgs;
    int _image_buffer_size {5};
    std::tuple<int, int> _pole_pos {std::make_tuple(250,250)};
    int _bounding_box_width {130};
    bool _pole_select_mode {false};
    TimeScrollBar* _time_scrollbar;
    std::vector<QGraphicsPathItem*> _contact_rectangle_items;
    std::vector<QGraphicsPixmapItem*> _contact_pixmaps;
    std::filesystem::path _output_path;
    int _highlighted_row {-1};

    void _buildContactTable();
    void _calculateContactPeriods();
    QImage::Format _getQImageFormat();
    void _drawContactRectangles(int frame_id);
    void _createContactRectangles();
    void _createContactPixmaps();
    void _saveContactBlocks();
    void _updateContactWidgets(int frame_id);

private slots:
    void _contactButton();
    void _noContactButton();
    void _saveContactFrameByFrame();
    void _loadContact();
    void _poleSelectButton();
    void _setBoundingBoxWidth(int value);
    void _flipContactButton();
    void _changeOutputDir();
    void _contactTableClicked(int row, int column);
};

int highlight_row(QTableWidget* table, int row_index, Qt::GlobalColor color);

#endif // CONTACT_WIDGET_HPP
