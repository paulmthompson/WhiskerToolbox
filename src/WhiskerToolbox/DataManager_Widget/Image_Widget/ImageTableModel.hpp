#ifndef WHISKERTOOLBOX_IMAGETABLEMODEL_HPP
#define WHISKERTOOLBOX_IMAGETABLEMODEL_HPP

#include <QAbstractTableModel>

#include <string>
#include <vector>

class ImageData;

struct ImageTableRow {
    int frame_index;
    std::string filename;
};

class ImageTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ImageTableModel(QObject * parent = nullptr);

    void setImages(ImageData const * imageData);

    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    [[nodiscard]] int getFrameForRow(int row) const;

private:
    std::vector<ImageTableRow> _display_data;
};

#endif//WHISKERTOOLBOX_IMAGETABLEMODEL_HPP