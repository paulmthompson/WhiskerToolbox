#ifndef DATAVIEW_COLUMNACCESSOR_HPP
#define DATAVIEW_COLUMNACCESSOR_HPP

#include "Analysis_Dashboard/DataView/Transforms/IDataViewTransform.hpp"

#include <optional>
#include <typeindex>
#include <vector>

// Forward declarations
class TableView;
class TableRegistry;

namespace DataViewColumns {

/**
 * @brief Load a scalar column as doubles from a table if row counts match
 */
inline std::optional<std::vector<double>> loadScalarAsDoubles(TableRegistry * registry,
                                                              QString const & tableId,
                                                              QString const & columnName,
                                                              size_t expectedRowCount) {
    if (!registry) return std::nullopt;
    auto view = registry->getBuiltTable(tableId);
    if (!view) return std::nullopt;
    try {
        auto type_idx = view->getColumnTypeIndex(columnName.toStdString());
        std::vector<double> out;
        if (type_idx == typeid(double)) {
            auto const & v = view->getColumnValues<double>(columnName.toStdString());
            if (v.size() != expectedRowCount) return std::nullopt;
            out.assign(v.begin(), v.end());
        } else if (type_idx == typeid(float)) {
            auto const & v = view->getColumnValues<float>(columnName.toStdString());
            if (v.size() != expectedRowCount) return std::nullopt;
            out.reserve(v.size());
            for (float x : v) out.push_back(static_cast<double>(x));
        } else if (type_idx == typeid(int)) {
            auto const & v = view->getColumnValues<int>(columnName.toStdString());
            if (v.size() != expectedRowCount) return std::nullopt;
            out.reserve(v.size());
            for (int x : v) out.push_back(static_cast<double>(x));
        } else if (type_idx == typeid(bool)) {
            auto const & v = view->getColumnValues<bool>(columnName.toStdString());
            if (v.size() != expectedRowCount) return std::nullopt;
            out.reserve(v.size());
            for (bool x : v) out.push_back(x ? 1.0 : 0.0);
        } else {
            return std::nullopt;
        }
        return out;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace DataViewColumns

#endif // DATAVIEW_COLUMNACCESSOR_HPP

