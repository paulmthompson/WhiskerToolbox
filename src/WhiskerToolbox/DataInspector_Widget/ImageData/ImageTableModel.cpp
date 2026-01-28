#include "ImageTableModel.hpp"

#include "DataManager/Media/Image_Data.hpp"

#include <iostream>

ImageTableModel::ImageTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

void ImageTableModel::setImages(ImageData const * imageData) {
    beginResetModel();
    _display_data.clear();

    if (imageData) {
        int const total_frames = imageData->getTotalFrameCount();
        _display_data.reserve(total_frames);

        for (int frame_index = 0; frame_index < total_frames; ++frame_index) {
            std::string filename = imageData->GetFrameID(frame_index);
            _display_data.push_back({frame_index, filename});
        }
    }

    endResetModel();
}

int ImageTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int ImageTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 2;// Frame Index, Filename
}

QVariant ImageTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= _display_data.size() || index.row() < 0) {
        return QVariant{};
    }

    ImageTableRow const & rowData = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(rowData.frame_index);
        case 1:
            return QString::fromStdString(rowData.filename);
        default:
            return QVariant{};
    }
}

QVariant ImageTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Frame");
            case 1:
                return QString("Filename");
            default:
                return QVariant{};
        }
    }
    return QVariant{};// No vertical header
}

int ImageTableModel::getFrameForRow(int row) const {
    if (row >= 0 && static_cast<size_t>(row) < _display_data.size()) {
        return _display_data[row].frame_index;
    }
    return -1;// Invalid row
}