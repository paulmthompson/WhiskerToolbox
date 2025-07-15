#include "Column.h"
#include "TableView.h"

#include <stdexcept>

Column::Column(std::string name, std::unique_ptr<IColumnComputer> computer)
    : m_name(std::move(name))
    , m_computer(std::move(computer))
    , m_cache(std::monostate{})
{
    if (!m_computer) {
        throw std::invalid_argument("IColumnComputer cannot be null");
    }
}

std::span<const double> Column::getSpan(TableView* table) {
    if (!table) {
        throw std::invalid_argument("TableView cannot be null");
    }

    // Check if data is already materialized
    if (!isMaterialized()) {
        materialize(table);
    }

    // Get the cached data
    const auto& cachedData = std::get<std::vector<double>>(m_cache);
    return std::span<const double>(cachedData);
}

void Column::materialize(TableView* table) {
    if (isMaterialized()) {
        return; // Already materialized
    }

    // Get the ExecutionPlan for this column's data source
    const auto& plan = table->getExecutionPlanFor(m_computer->getSourceDependency());

    // Compute the column values
    auto computedValues = m_computer->compute(plan);

    // Store in cache
    m_cache = std::move(computedValues);
}
