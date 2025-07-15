#include "Column.h"
#include "utils/TableView/core/TableView.h"

#include <stdexcept>

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



// Explicit instantiation for commonly used types
template class Column<double>;
template class Column<bool>;
template class Column<int>;
template class Column<std::vector<double>>;
template class Column<std::vector<int>>;
template class Column<std::vector<TimeFrameIndex>>;
