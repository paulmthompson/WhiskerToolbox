#include "Column.h"

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/core/TableView.h"

template<SupportedColumnType T>
auto Column<T>::getValues(TableView * table) -> std::vector<T> const & {
    if (!isMaterialized()) {
        materialize(table);
    }
    return std::get<std::vector<T>>(m_cache);
}

template<SupportedColumnType T>
void Column<T>::materialize(TableView * table) {
    if (isMaterialized()) {
        return;
    }

    ExecutionPlan const & plan = table->getExecutionPlanFor(getSourceDependency());

    // Compute the column values using the computer
    auto computed = m_computer->compute(plan);

    // Store the computed values in the cache
    m_cache = std::move(computed);
}


// Explicit instantiation for commonly used types
template class Column<double>;
template class Column<float>;
template class Column<std::vector<float>>;
template class Column<bool>;
template class Column<int>;
template class Column<int64_t>;
template class Column<std::vector<double>>;
template class Column<std::vector<int>>;
template class Column<std::vector<TimeFrameIndex>>;
