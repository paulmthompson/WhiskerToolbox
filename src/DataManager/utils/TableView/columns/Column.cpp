#include "Column.h"

#include "utils/TableView/core/TableView.h"
#include "utils/TableView/interfaces/IColumnComputer.h"

#include <iostream>

template<SupportedColumnType T>
Column<T>::Column(std::string name, std::unique_ptr<IColumnComputer<T>> computer)
    : m_name(std::move(name)),
      m_computer(std::move(computer)){};

template<SupportedColumnType T>
std::vector<T> const & Column<T>::getValues(TableView * table) {
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
    auto [computed, entity_ids] = m_computer->compute(plan);

    // Store the computed values in the cache
    m_cache = std::move(computed);
    m_entityIds = std::move(entity_ids);
}

template<SupportedColumnType T>
std::string Column<T>::getSourceDependency() const {
    return m_computer->getSourceDependency();
}


template<SupportedColumnType T>
std::vector<std::string> Column<T>::getDependencies() const {
    return m_computer->getDependencies();
}

template<SupportedColumnType T>
bool Column<T>::isMaterialized() const {
    return std::holds_alternative<std::vector<T>>(m_cache);
}

template<SupportedColumnType T>
EntityIdStructure Column<T>::getEntityIdStructure() const {
    return m_computer->getEntityIdStructure();
}

template<SupportedColumnType T>
std::vector<EntityId> Column<T>::getCellEntityIds(size_t row_index) const {
    if (getEntityIdStructure() == EntityIdStructure::None) {
        return {};
    } else if (getEntityIdStructure() == EntityIdStructure::Simple) {
        return {std::get<std::vector<EntityId>>(m_entityIds).at(row_index)};
    } else if (getEntityIdStructure() == EntityIdStructure::Complex) {
        return std::get<std::vector<std::vector<EntityId>>>(m_entityIds).at(row_index);
    } else {
        return {};
    }
}

template<SupportedColumnType T>
ColumnEntityIds Column<T>::getColumnEntityIds() const {
    if (!isMaterialized()) {
        std::cout << "Column " << m_name << " is not materialized" << std::endl;
    }
    return m_entityIds;
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
