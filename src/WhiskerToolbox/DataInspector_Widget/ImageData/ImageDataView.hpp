#ifndef IMAGE_DATA_VIEW_HPP
#define IMAGE_DATA_VIEW_HPP

/**
 * @file ImageDataView.hpp
 * @brief Table view widget for ImageData (Images/Video)
 * 
 * ImageDataView provides a table view for ImageData objects in the Center zone.
 * It displays image data in a table format with columns for frame index and
 * filename.
 * 
 * @see BaseDataView for the base class
 * @see ImageTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include <memory>

class ImageTableModel;

class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for ImageData (Images/Video)
 */
class ImageDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit ImageDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~ImageDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Images; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Image Table"); }

    [[nodiscard]] std::vector<int> getSelectedFrames() const;
    [[nodiscard]] QTableView * tableView() const { return _table_view; }

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();

private:
    void _setupUi();
    void _connectSignals();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    ImageTableModel * _table_model{nullptr};
    int _callback_id{-1};
};

#endif // IMAGE_DATA_VIEW_HPP
