#ifndef TENSORTABLEMODEL_HPP
#define TENSORTABLEMODEL_HPP

#include <QAbstractTableModel>

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include <torch/torch.h>
#define slots Q_SLOTS

#include <map>

class TensorTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    TensorTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setTensors(const std::map<int, torch::Tensor>& tensors) {
        beginResetModel();
        _tensors = tensors;
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_tensors.size());
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 2; // Frame and Shape columns
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant();
        }

        auto it = std::next(_tensors.begin(), index.row());
        const int frame = it->first;
        const torch::Tensor &tensor = it->second;

        if (index.column() == 0) {
            return QVariant::fromValue(frame);
        } else if (index.column() == 1) {

            auto tensor_size = tensor.sizes();
            QString shape = QString::number(tensor_size[0]);
            for (int i = 1; i < tensor_size.size(); i++) {
                shape += "x" + QString::number(tensor_size[i]);
            }
            return shape;
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole) {
            return QVariant();
        }

        if (orientation == Qt::Horizontal) {
            if (section == 0) {
                return QString("Frame");
            } else if (section == 1) {
                return QString("Shape");
            }
        }

        return QVariant();
    }

private:
    std::map<int, torch::Tensor> _tensors;
};

#endif // TENSORTABLEMODEL_HPP
