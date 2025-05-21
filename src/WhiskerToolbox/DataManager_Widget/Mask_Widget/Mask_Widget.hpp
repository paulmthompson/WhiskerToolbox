#ifndef MASK_WIDGET_HPP
#define MASK_WIDGET_HPP

#include <QWidget>
#include <QModelIndex>

#include <filesystem>
#include <memory>
#include <string>

namespace dl {
class EfficientSAM;
};

namespace Ui {
class Mask_Widget;
}

class DataManager;
class MaskTableModel;

class Mask_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Mask_Widget() override;

    void openWidget();// Call
    void selectPoint(float x, float y);
    void setActiveKey(std::string const & key);
    void removeCallbacks();
    void updateTable();

signals:
    void frameSelected(int frame_id);

private:
    Ui::Mask_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<dl::EfficientSAM> _sam_model;
    std::string _active_key;
    MaskTableModel * _mask_table_model;
    int _callback_id{-1};

    void _populateMoveToMaskDataComboBox();

private slots:
    void _loadSamModel();
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _moveMasksButton_clicked();
    void _deleteMasksButton_clicked();
    void _onDataChanged();
};


#endif// MASK_WIDGET_HPP
