#ifndef TENSORTABLEMODEL_HPP
#define TENSORTABLEMODEL_HPP

#include "DataManager/TimeFrame/TimeFrame.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include <QAbstractTableModel>

#include <map>

class TensorData; // Forward declaration

class TensorTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit TensorTableModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent), _tensor_data(nullptr) {}

    void setTensorData(TensorData* tensor_data) {
        beginResetModel();
        _tensor_data = tensor_data;
        if (_tensor_data) {
            _frame_indices = _tensor_data->getTimesWithTensors();
        } else {
            _frame_indices.clear();
        }
        endResetModel();
    }

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_frame_indices.size());
    }

    [[nodiscard]] int columnCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return 2;// Frame and Shape columns
    }

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override {
        if (!index.isValid() || role != Qt::DisplayRole || !_tensor_data) {
            return QVariant{};
        }

        if (index.row() >= static_cast<int>(_frame_indices.size())) {
            return QVariant{};
        }

        auto const frame = _frame_indices[index.row()];

        if (index.column() == 0) {
            return QVariant::fromValue(frame.getValue());
        } else if (index.column() == 1) {
            auto tensor_shape = _tensor_data->getTensorShapeAtTime(frame);
            if (tensor_shape.empty()) {
                return QString("Unknown");
            }
            
            QString shape = QString::number(tensor_shape[0]);
            for (size_t i = 1; i < tensor_shape.size(); i++) {
                shape += "x" + QString::number(tensor_shape[i]);
            }
            return shape;
        }

        return QVariant{};
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole) {
            return QVariant{};
        }

        if (orientation == Qt::Horizontal) {
            if (section == 0) {
                return QString("Frame");
            } else if (section == 1) {
                return QString("Shape");
            }
        }

        return QVariant{};
    }

private:
    TensorData* _tensor_data;
    std::vector<TimeFrameIndex> _frame_indices;
};

#endif// TENSORTABLEMODEL_HPP
