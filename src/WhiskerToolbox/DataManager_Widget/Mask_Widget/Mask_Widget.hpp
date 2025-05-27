#ifndef MASK_WIDGET_HPP
#define MASK_WIDGET_HPP

#include "DataManager/Masks/IO/Image/Mask_Data_Image.hpp"

#include <QModelIndex>
#include <QWidget>

#include <memory>
#include <string>
#include <variant>

namespace dl {
class EfficientSAM;
};

namespace Ui {
class Mask_Widget;
}

class DataManager;
class MaskTableModel;
class ImageMaskSaver_Widget;
class HDF5MaskSaver_Widget;
class MediaExport_Widget;

using MaskSaverOptionsVariant = std::variant<ImageMaskSaverOptions>;

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

    enum SaverType { HDF5,
                     IMAGE };
    void _initiateSaveProcess(SaverType saver_type, MaskSaverOptionsVariant & options_variant);
    bool _performActualImageSave(ImageMaskSaverOptions & options);

private slots:
    void _loadSamModel();
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _moveMasksButton_clicked();
    void _deleteMasksButton_clicked();
    void _onDataChanged();

    // Export slots
    void _onExportTypeChanged(int index);
    void _handleSaveImageMaskRequested(ImageMaskSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);
};


#endif// MASK_WIDGET_HPP
