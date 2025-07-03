#ifndef IMAGE_WIDGET_HPP
#define IMAGE_WIDGET_HPP

#include <QModelIndex>
#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class Image_Widget;
}

class DataManager;
class ImageTableModel;

class Image_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Image_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Image_Widget() override;

    void openWidget();
    void setActiveKey(std::string const & key);
    void removeCallbacks();
    void updateTable();

signals:
    void frameSelected(int frame_id);

private:
    Ui::Image_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;
    ImageTableModel * _image_table_model;
    int _callback_id{-1};

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
};

#endif// IMAGE_WIDGET_HPP