#ifndef TIMEFRAME_TABLE_MODEL_HPP
#define TIMEFRAME_TABLE_MODEL_HPP

/**
 * @file TimeFrameTableModel.hpp
 * @brief Table model for displaying TimeFrame values
 *
 * Displays each entry in a TimeFrame as a row with two columns:
 * - Index: the TimeFrameIndex (0-based position)
 * - Time: the absolute time value stored at that index
 *
 * @see TimeFrame for the underlying data
 * @see TimeFrameDataView for the view widget that uses this model
 */

#include <QAbstractTableModel>

#include <vector>

class TimeFrameTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    /// Column indices
    enum Column {
        IndexColumn = 0,
        TimeColumn = 1,
        ColumnCount = 2
    };

    explicit TimeFrameTableModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent) {}

    /**
     * @brief Set the time values to display
     * @param times Vector of time values (index in vector = TimeFrameIndex)
     */
    void setTimeValues(std::vector<int> const & times) {
        beginResetModel();
        _times = times;
        endResetModel();
    }

    /**
     * @brief Get the index for a given row
     * @param row Row in the table
     * @return The TimeFrameIndex value (row position)
     */
    [[nodiscard]] int64_t getIndex(int row) const {
        return row;
    }

    /**
     * @brief Get the time value for a given row
     * @param row Row in the table
     * @return The stored time value
     */
    [[nodiscard]] int getTimeValue(int row) const {
        return _times.at(static_cast<size_t>(row));
    }

    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_times.size());
    }

    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return ColumnCount;
    }

    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant{};
        }

        auto const row = static_cast<size_t>(index.row());
        if (row >= _times.size()) {
            return QVariant{};
        }

        switch (index.column()) {
            case IndexColumn:
                return QVariant::fromValue(static_cast<qlonglong>(row));
            case TimeColumn:
                return QVariant::fromValue(_times[row]);
            default:
                return QVariant{};
        }
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
            return QVariant{};
        }

        switch (section) {
            case IndexColumn:
                return QStringLiteral("Index");
            case TimeColumn:
                return QStringLiteral("Time");
            default:
                return QVariant{};
        }
    }

private:
    std::vector<int> _times;
};

#endif // TIMEFRAME_TABLE_MODEL_HPP
