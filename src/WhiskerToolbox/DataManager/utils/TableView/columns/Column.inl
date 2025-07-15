// Column.inl - Template method implementations
// This file is included at the end of Column.h to avoid circular dependencies

#ifndef COLUMN_INL_INCLUDED
#define COLUMN_INL_INCLUDED

#include "utils/TableView/core/TableView.h"

template<typename T>
auto Column<T>::getValues(TableView* table) -> const std::vector<T>& {
    if (!isMaterialized()) {
        materialize(table);
    }
    return std::get<std::vector<T>>(m_cache);
}

template<typename T>
void Column<T>::materialize(TableView* table) {
    if (isMaterialized()) {
        return;
    }

    // Get the execution plan for this column's data source
    const auto& plan = table->getExecutionPlanFor(getSourceDependency());
    
    // Compute the column values using the computer
    auto computed = m_computer->compute(plan);
    
    // Store the computed values in the cache
    m_cache = std::move(computed);
}

#endif // COLUMN_INL_INCLUDED
