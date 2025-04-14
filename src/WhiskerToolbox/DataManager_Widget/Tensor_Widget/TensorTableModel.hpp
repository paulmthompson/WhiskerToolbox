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
    explicit TensorTableModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent) {}

    void setTensors(std::map<int, torch::Tensor> const & tensors) {
        beginResetModel();
        _tensors = tensors;
        endResetModel();
    }

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_tensors.size());
    }

    [[nodiscard]] int columnCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return 2;// Frame and Shape columns
    }

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant{};
        }

        auto it = std::next(_tensors.begin(), index.row());
        int const frame = it->first;
        torch::Tensor const & tensor = it->second;

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
    std::map<int, torch::Tensor> _tensors;
};

#endif// TENSORTABLEMODEL_HPP
