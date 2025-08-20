#ifndef MASK_LOADER_WIDGET_HPP
#define MASK_LOADER_WIDGET_HPP


#include "IO_Widgets/Masks/HDF5/HDF5MaskLoader_Widget.hpp"
#include "IO_Widgets/Masks/Image/ImageMaskLoader_Widget.hpp"
#include "IO_Widgets/Scaling_Widget/Scaling_Widget.hpp"

#include <QWidget>
#include <QString>
#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class Mask_Loader_Widget;
}

class Mask_Loader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Mask_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Mask_Loader_Widget() override;

private:
    Ui::Mask_Loader_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    // These methods will now be called by signals from HDF5MaskLoader_Widget or other specific loaders
    void _loadSingleHDF5MaskFile(std::string const & filename, std::string const & mask_suffix = ""); 
    void _loadMultiHDF5MaskFiles(QString const& directory_path, QString const& pattern);


private slots:
    void _onLoaderTypeChanged(int index);
    void _handleSingleHDF5LoadRequested();
    void _handleMultiHDF5LoadRequested(QString pattern);
    void _handleImageMaskLoadRequested(QString format, nlohmann::json config);

};

#endif// MASK_LOADER_WIDGET_HPP 
