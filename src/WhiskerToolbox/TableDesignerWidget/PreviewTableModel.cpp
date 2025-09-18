#include "PreviewTableModel.hpp"

#include "DataManager/utils/TableView/core/TableView.h"

#include <QtCore/QVariant>

#include <iomanip>
#include <sstream>
#include <type_traits>

PreviewTableModel::PreviewTableModel(QObject * parent)
    : QAbstractTableModel(parent), _column_names() {}

void PreviewTableModel::setPreview(std::shared_ptr<TableView> view) {
    beginResetModel();
    _preview_view = std::move(view);
    _column_names.clear();
    if (_preview_view) {
        auto names = _preview_view->getColumnNames();
        for (auto const & n : names) _column_names.push_back(QString::fromStdString(n));
    }
    endResetModel();
}

void PreviewTableModel::clearPreview() {
    beginResetModel();
    _preview_view.reset();
    _column_names.clear();
    endResetModel();
}

int PreviewTableModel::rowCount(QModelIndex const & parent) const {
    if (parent.isValid() || !_preview_view) return 0;
    return static_cast<int>(_preview_view->getRowCount());
}

int PreviewTableModel::columnCount(QModelIndex const & parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(_column_names.size());
}

QVariant PreviewTableModel::data(QModelIndex const & index, int role) const {
    if (role != Qt::DisplayRole || !_preview_view || !index.isValid()) return {};
    if (index.row() < 0 || index.column() < 0) return {};
    if (index.column() >= _column_names.size()) return {};

    auto const colName = _column_names[index.column()].toStdString();
    auto const row = static_cast<size_t>(index.row());

    try {
        // Use visitColumnData to avoid try/catch per type detection
        auto * view = const_cast<TableView *>(_preview_view.get());
        auto text = view->visitColumnData(colName, [&](auto const & vec) -> QString {
            using VecT = std::decay_t<decltype(vec)>;
            using ElemT = typename VecT::value_type;

            if (row >= vec.size()) {
                if constexpr (std::is_same_v<ElemT, double>) return QString("NaN");
                if constexpr (std::is_same_v<ElemT, int>) return QString("NaN");
                if constexpr (std::is_same_v<ElemT, bool>) return QString("false");
            }

            if constexpr (std::is_same_v<ElemT, double>) {
                return formatScalarDouble(vec[row]);
            } else if constexpr (std::is_same_v<ElemT, int>) {
                return formatScalarInt(vec[row]);
            } else if constexpr (std::is_same_v<ElemT, bool>) {
                return formatScalarBool(vec[row]);
            } else if constexpr (std::is_same_v<ElemT, std::vector<double>>) {
                return joinVector(vec[row]);
            } else if constexpr (std::is_same_v<ElemT, std::vector<int>>) {
                return joinVector(vec[row]);
            } else if constexpr (std::is_same_v<ElemT, std::vector<float>>) {
                return joinVector(vec[row]);
            } else {
                return QString("?");
            }
        });
        return text;
    } catch (...) {
        return {};
    }
}

QVariant PreviewTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section < _column_names.size()) return _column_names[section];
        return {};
    }
    return section + 1; // 1-based row numbering
}

QString PreviewTableModel::formatScalarBool(bool value) { return value ? QString("true") : QString("false"); }

QString PreviewTableModel::formatScalarInt(int value) { return QString::number(value); }

QString PreviewTableModel::formatScalarDouble(double value) {
    // Sensible default: fixed with 3 decimals
    QString s;
    s.setNum(value, 'f', 3);
    return s;
}

template<typename T>
QString PreviewTableModel::joinVector(std::vector<T> const & values) {
    if (values.empty()) return QString();
    QString out;
    out.reserve(static_cast<int>(values.size() * 4));
    for (size_t i = 0; i < values.size(); ++i) {
        if constexpr (std::is_same_v<T, double>) {
            out += formatScalarDouble(values[i]);
        } else if constexpr (std::is_same_v<T, float>) {
            out += formatScalarDouble(static_cast<double>(values[i]));
        } else {
            out += QString::number(values[i]);
        }
        if (i + 1 < values.size()) out += ",";
    }
    return out;
}

// Explicit instantiations
template QString PreviewTableModel::joinVector<double>(std::vector<double> const &);
template QString PreviewTableModel::joinVector<int>(std::vector<int> const &);
template QString PreviewTableModel::joinVector<float>(std::vector<float> const &);


