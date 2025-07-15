#include "TableView.h"

#include <algorithm>
#include <set>
#include <stdexcept>

TableView::TableView(std::unique_ptr<IRowSelector> rowSelector, 
                     std::shared_ptr<DataManagerExtension> dataManager)
    : m_rowSelector(std::move(rowSelector))
    , m_dataManager(std::move(dataManager))
{
    if (!m_rowSelector) {
        throw std::invalid_argument("IRowSelector cannot be null");
    }
    if (!m_dataManager) {
        throw std::invalid_argument("DataManagerExtension cannot be null");
    }
}

size_t TableView::getRowCount() const {
    return m_rowSelector->getRowCount();
}

size_t TableView::getColumnCount() const {
    return m_columns.size();
}

std::span<const double> TableView::getColumnSpan(const std::string& name) {
    // This method is maintained for backward compatibility with double columns
    return std::span<const double>(getColumnValues<double>(name));
}

std::vector<std::string> TableView::getColumnNames() const {
    std::vector<std::string> names;
    names.reserve(m_columns.size());
    
    for (const auto& column : m_columns) {
        names.push_back(column->getName());
    }
    
    return names;
}

bool TableView::hasColumn(const std::string& name) const {
    return m_colNameToIndex.find(name) != m_colNameToIndex.end();
}

void TableView::materializeAll() {
    std::set<std::string> materializing;
    
    for (const auto& column : m_columns) {
        if (!column->isMaterialized()) {
            materializeColumn(column->getName(), materializing);
        }
    }
}

void TableView::clearCache() {
    // Clear column caches
    for (auto& column : m_columns) {
        column->clearCache();
    }
    
    // Clear execution plan cache
    m_planCache.clear();
}

const ExecutionPlan& TableView::getExecutionPlanFor(const std::string& sourceName) {
    // Check cache first
    auto it = m_planCache.find(sourceName);
    if (it != m_planCache.end()) {
        return it->second;
    }

    // Generate new plan
    ExecutionPlan plan = generateExecutionPlan(sourceName);
    
    // Store in cache and return reference
    auto [insertedIt, inserted] = m_planCache.emplace(sourceName, std::move(plan));
    if (!inserted) {
        throw std::runtime_error("Failed to cache ExecutionPlan for source: " + sourceName);
    }
    
    return insertedIt->second;
}

void TableView::addColumn(std::shared_ptr<IColumn> column) {
    if (!column) {
        throw std::invalid_argument("Column cannot be null");
    }

    const std::string& name = column->getName();
    
    // Check for duplicate names
    if (hasColumn(name)) {
        throw std::runtime_error("Column '" + name + "' already exists");
    }

    // Add to collections
    size_t index = m_columns.size();
    m_columns.push_back(std::move(column));
    m_colNameToIndex[name] = index;
}

void TableView::materializeColumn(const std::string& columnName, std::set<std::string>& materializing) {
    // Check for circular dependencies
    if (materializing.find(columnName) != materializing.end()) {
        throw std::runtime_error("Circular dependency detected involving column: " + columnName);
    }

    // Check if column exists
    if (!hasColumn(columnName)) {
        throw std::runtime_error("Column '" + columnName + "' not found");
    }

    auto& column = m_columns[m_colNameToIndex[columnName]];
    
    // If already materialized, nothing to do
    if (column->isMaterialized()) {
        return;
    }

    // Mark as being materialized
    materializing.insert(columnName);

    // Materialize dependencies first
    const auto dependencies = column->getDependencies();
    for (const auto& dependency : dependencies) {
        if (hasColumn(dependency)) {
            materializeColumn(dependency, materializing);
        }
    }

    // Materialize this column
    column->materialize(this); // Use the IColumn interface method

    // Remove from materializing set
    materializing.erase(columnName);
}

ExecutionPlan TableView::generateExecutionPlan(const std::string& sourceName) {
    // Get the data source to understand its structure
    auto dataSource = m_dataManager->getAnalogSource(sourceName);
    if (!dataSource) {
        throw std::runtime_error("Data source '" + sourceName + "' not found");
    }

    // Generate plan based on row selector type
    // For now, we'll use a simple approach based on the row selector type
    
    // Check if we have an IntervalSelector
    if (auto intervalSelector = dynamic_cast<IntervalSelector*>(m_rowSelector.get())) {
        // Convert intervals to TimeFrameInterval format
        const auto& intervals = intervalSelector->getIntervals();
        return ExecutionPlan(intervals);
    }
    
    // Check if we have a TimestampSelector
    if (auto timestampSelector = dynamic_cast<TimestampSelector*>(m_rowSelector.get())) {
        // Convert timestamps to TimeFrameIndex format
        const auto& timestamps = timestampSelector->getTimestamps();
        std::vector<TimeFrameIndex> indices;
        indices.reserve(timestamps.size());
        
        for (double timestamp : timestamps) {
            // For now, assume simple conversion (this would need proper time-to-index conversion)
            indices.emplace_back(static_cast<int64_t>(timestamp));
        }
        
        return ExecutionPlan(std::move(indices));
    }
    
    // Check if we have an IndexSelector
    if (auto indexSelector = dynamic_cast<IndexSelector*>(m_rowSelector.get())) {
        // Convert size_t indices to TimeFrameIndex format
        const auto& indices = indexSelector->getIndices();
        std::vector<TimeFrameIndex> timeFrameIndices;
        timeFrameIndices.reserve(indices.size());
        
        for (size_t index : indices) {
            timeFrameIndices.emplace_back(static_cast<int64_t>(index));
        }
        
        return ExecutionPlan(std::move(timeFrameIndices));
    }
    
    throw std::runtime_error("Unsupported row selector type for source: " + sourceName);
}

RowDescriptor TableView::getRowDescriptor(size_t row_index) const {
    if (m_rowSelector) {
        return m_rowSelector->getDescriptor(row_index);
    }
    return std::monostate{};
}
