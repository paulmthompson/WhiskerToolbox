#ifndef EVENTTABLEMODEL_HPP
#define EVENTTABLEMODEL_HPP


#include <QAbstractTableModel>

#include <vector>

class EventTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit EventTableModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent) {}

    void setEvents(std::vector<float> const & events) {
        beginResetModel();
        _events = events;
        endResetModel();
    }

    [[nodiscard]] float getEvent(int row) const {
        return _events[row];
    }

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_events.size());
    }

    [[nodiscard]] int columnCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return 1;// Single column for event time
    }

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant{};
        }

        if (index.column() == 0) {
            return QVariant::fromValue(_events.at(index.row()));
        }

        return QVariant{};
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole) {
            return QVariant{};
        }

        if (orientation == Qt::Horizontal && section == 0) {
            return QString("Frame");
        }

        return QVariant{};
    }

    [[nodiscard]] Qt::ItemFlags flags(QModelIndex const & index) const override {
        if (!index.isValid()) {
            return Qt::ItemIsEnabled;
        }
        return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
    }

    bool setData(QModelIndex const & index, QVariant const & value, int role) override {
        if (index.isValid() && role == Qt::EditRole && index.column() == 0) {
            _events[index.row()] = value.toFloat();
            emit dataChanged(index, index, {role});
            return true;
        }
        return false;
    }

private:
    std::vector<float> _events;
};

#endif// EVENTTABLEMODEL_HPP
