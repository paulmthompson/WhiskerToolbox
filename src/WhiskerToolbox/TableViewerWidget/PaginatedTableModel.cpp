#include "PaginatedTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/TableInfo.hpp"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/TableRegistry.hpp"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/DataManager.hpp"

#include <QDebug>
#include <algorithm>
#include <iomanip>
#include <sstream>

PaginatedTableModel::PaginatedTableModel(QObject * parent)
    : QAbstractTableModel(parent) {
}

PaginatedTableModel::~PaginatedTableModel() = default;

void PaginatedTableModel::setSourceTable(std::unique_ptr<IRowSelector> row_selector,
                                        std::vector<ColumnInfo> column_infos,
                                        std::shared_ptr<DataManager> data_manager) {
    beginResetModel();
    
    _source_row_selector = std::move(row_selector);
    _column_infos = std::move(column_infos);
    _data_manager = data_manager;
    _complete_table_view.reset();
    
    // Calculate total rows and extract column names
    if (_source_row_selector) {
        _total_rows = _source_row_selector->getRowCount();
    } else {
        _total_rows = 0;
    }
    
    _column_names.clear();
    for (auto const & col_info : _column_infos) {
        _column_names.push_back(QString::fromStdString(col_info.name));
    }
    
    // Clear cache
    _page_cache.clear();
    
    endResetModel();
}

void PaginatedTableModel::setTableView(std::shared_ptr<TableView> table_view) {
    beginResetModel();
    
    _complete_table_view = std::move(table_view);
    _source_row_selector.reset();
    _column_infos.clear();
    _data_manager.reset();
    
    if (_complete_table_view) {
        _total_rows = _complete_table_view->getRowCount();
        auto names = _complete_table_view->getColumnNames();
        _column_names.clear();
        for (auto const & name : names) {
            _column_names.push_back(QString::fromStdString(name));
        }
    } else {
        _total_rows = 0;
        _column_names.clear();
    }
    
    // Clear cache
    _page_cache.clear();
    
    endResetModel();
}

void PaginatedTableModel::clearTable() {
    beginResetModel();
    _source_row_selector.reset();
    _column_infos.clear();
    _data_manager.reset();
    _complete_table_view.reset();
    _total_rows = 0;
    _column_names.clear();
    _page_cache.clear();
    endResetModel();
}

void PaginatedTableModel::setPageSize(size_t page_size) {
    if (page_size == 0 || page_size == _page_size) return;
    
    beginResetModel();
    _page_size = page_size;
    _page_cache.clear(); // Clear cache since page boundaries changed
    endResetModel();
}

int PaginatedTableModel::rowCount(QModelIndex const & parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(_total_rows);
}

int PaginatedTableModel::columnCount(QModelIndex const & parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(_column_names.size());
}

QVariant PaginatedTableModel::data(QModelIndex const & index, int role) const {
    if (role != Qt::DisplayRole || !index.isValid()) return {};
    if (index.row() < 0 || index.column() < 0) return {};
    if (index.row() >= static_cast<int>(_total_rows)) return {};
    if (index.column() >= _column_names.size()) return {};

    auto const row = static_cast<size_t>(index.row());
    auto const column_name = _column_names[index.column()].toStdString();

    try {
        if (_complete_table_view) {
            // Use the complete table view directly
            return formatValue(_complete_table_view, column_name, row);
        } else {
            // Use pagination with mini tables
            auto [mini_table, local_row] = getMiniTableForRow(row);
            if (!mini_table || local_row >= mini_table->getRowCount()) {
                return QString("Error");
            }
            return formatValue(mini_table, column_name, local_row);
        }
    } catch (std::exception const & e) {
        qDebug() << "Error accessing data at row" << row << "column" << column_name.c_str() << ":" << e.what();
        return QString("Error");
    }
}

QVariant PaginatedTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section < _column_names.size()) {
            return _column_names[section];
        }
        return {};
    }
    return section + 1; // 1-based row numbering
}

std::pair<std::shared_ptr<TableView>, size_t> PaginatedTableModel::getMiniTableForRow(size_t row_index) const {
    if (!_source_row_selector || !_data_manager) {
        return {nullptr, 0};
    }
    
    // Calculate which page this row belongs to
    size_t page_number = row_index / _page_size;
    size_t page_start_row = page_number * _page_size;
    size_t local_row = row_index - page_start_row;
    
    // Check cache first
    auto cache_it = _page_cache.find(page_number);
    if (cache_it != _page_cache.end()) {
        return {cache_it->second, local_row};
    }
    
    // Create mini table for this page
    size_t actual_page_size = std::min(_page_size, _total_rows - page_start_row);
    auto mini_table = createMiniTable(page_start_row, actual_page_size);
    
    if (mini_table) {
        // Cache the mini table
        _page_cache[page_number] = mini_table;
        cleanupCache();
    }
    
    return {mini_table, local_row};
}

std::shared_ptr<TableView> PaginatedTableModel::createMiniTable(size_t page_start_row, size_t page_size) const {
    if (!_source_row_selector || !_data_manager) {
        return nullptr;
    }
    
    try {
        // Get the data manager extension and table registry
        auto* table_registry = _data_manager->getTableRegistry();
        if (!table_registry) {
            qDebug() << "Failed to get table registry from data manager";
            return nullptr;
        }
        
        auto data_manager_extension = table_registry->getDataManagerExtension();
        if (!data_manager_extension) {
            qDebug() << "Failed to get data manager extension from table registry";
            return nullptr;
        }
        // Create a vector of indices for this window
        std::vector<size_t> window_indices;
        window_indices.reserve(page_size);
        for (size_t i = 0; i < page_size && (page_start_row + i) < _total_rows; ++i) {
            window_indices.push_back(page_start_row + i);
        }
        
        // Use the existing cloneRowSelectorFiltered pattern
        auto windowed_selector = [&]() -> std::unique_ptr<IRowSelector> {
            // Create filtered selector based on the type of the source selector
            if (auto indexSelector = dynamic_cast<IndexSelector const *>(_source_row_selector.get())) {
                std::vector<size_t> const & source_indices = indexSelector->getIndices();
                std::vector<size_t> filtered;
                filtered.reserve(window_indices.size());
                for (size_t k : window_indices) {
                    if (k < source_indices.size()) {
                        filtered.push_back(source_indices[k]);
                    }
                }
                return std::make_unique<IndexSelector>(std::move(filtered));
            }
            
            if (auto timestampSelector = dynamic_cast<TimestampSelector const *>(_source_row_selector.get())) {
                auto const & timestamps = timestampSelector->getTimestamps();
                std::vector<TimeFrameIndex> filtered;
                filtered.reserve(window_indices.size());
                for (size_t k : window_indices) {
                    if (k < timestamps.size()) {
                        filtered.push_back(timestamps[k]);
                    }
                }
                return std::make_unique<TimestampSelector>(std::move(filtered), timestampSelector->getTimeFrame());
            }
            
            if (auto intervalSelector = dynamic_cast<IntervalSelector const *>(_source_row_selector.get())) {
                auto const & intervals = intervalSelector->getIntervals();
                std::vector<TimeFrameInterval> filtered;
                filtered.reserve(window_indices.size());
                for (size_t k : window_indices) {
                    if (k < intervals.size()) {
                        filtered.push_back(intervals[k]);
                    }
                }
                return std::make_unique<IntervalSelector>(std::move(filtered), intervalSelector->getTimeFrame());
            }
            
            return nullptr;
        }();
        
        if (!windowed_selector) {
            qDebug() << "Failed to create windowed selector for page starting at row" << page_start_row;
            return nullptr;
        }
        
        // Build a mini table with the windowed selector
        TableViewBuilder builder(data_manager_extension);
        builder.setRowSelector(std::move(windowed_selector));
        
        // Add all columns using the TableRegistry method
        for (auto const & col_info : _column_infos) {
            if (!table_registry->addColumnToBuilder(builder, col_info)) {
                qDebug() << "Failed to add column" << col_info.name.c_str() << "to mini table";
                return nullptr;
            }
        }
        
        auto mini_table_obj = builder.build();
        auto mini_table = std::make_shared<TableView>(std::move(mini_table_obj));
        mini_table->materializeAll();
        
        return mini_table;
    } catch (std::exception const & e) {
        qDebug() << "Exception creating mini table:" << e.what();
        return nullptr;
    }
}

void PaginatedTableModel::cleanupCache() const {
    while (_page_cache.size() > MAX_CACHED_PAGES) {
        // Remove the first (oldest) entry
        _page_cache.erase(_page_cache.begin());
    }
}

QString PaginatedTableModel::formatValue(std::shared_ptr<TableView> const & mini_table, 
                                       std::string const & column_name, 
                                       size_t local_row) {
    if (!mini_table || local_row >= mini_table->getRowCount()) {
        return QString("N/A");
    }
    
    try {
        return mini_table->visitColumnData(column_name, [local_row](auto const & vec) -> QString {
            using VecT = std::decay_t<decltype(vec)>;
            using ElemT = typename VecT::value_type;

            if (local_row >= vec.size()) {
                if constexpr (std::is_same_v<ElemT, double>) return QString("NaN");
                if constexpr (std::is_same_v<ElemT, int>) return QString("NaN");
                if constexpr (std::is_same_v<ElemT, bool>) return QString("false");
                return QString("N/A");
            }

            if constexpr (std::is_same_v<ElemT, double>) {
                return QString::number(vec[local_row], 'f', 3);
            } else if constexpr (std::is_same_v<ElemT, int>) {
                return QString::number(vec[local_row]);
            } else if constexpr (std::is_same_v<ElemT, bool>) {
                return vec[local_row] ? QString("true") : QString("false");
            } else if constexpr (std::is_same_v<ElemT, std::vector<double>>) {
                return joinVector(vec[local_row]);
            } else if constexpr (std::is_same_v<ElemT, std::vector<int>>) {
                return joinVector(vec[local_row]);
            } else if constexpr (std::is_same_v<ElemT, std::vector<float>>) {
                return joinVector(vec[local_row]);
            } else {
                return QString("?");
            }
        });
    } catch (...) {
        return QString("Error");
    }
}

template<typename T>
QString PaginatedTableModel::joinVector(std::vector<T> const & values) {
    if (values.empty()) return QString();
    QString out;
    out.reserve(static_cast<int>(values.size() * 4));
    for (size_t i = 0; i < values.size(); ++i) {
        if constexpr (std::is_same_v<T, double>) {
            out += QString::number(values[i], 'f', 3);
        } else if constexpr (std::is_same_v<T, float>) {
            out += QString::number(static_cast<double>(values[i]), 'f', 3);
        } else {
            out += QString::number(values[i]);
        }
        if (i + 1 < values.size()) out += ",";
    }
    return out;
}

// Explicit instantiations
template QString PaginatedTableModel::joinVector<double>(std::vector<double> const &);
template QString PaginatedTableModel::joinVector<int>(std::vector<int> const &);
template QString PaginatedTableModel::joinVector<float>(std::vector<float> const &);

#include "PaginatedTableModel.moc"
