#include "TableView.h"

#include <algorithm>
#include <set>
#include <stdexcept>

TableView::TableView(std::unique_ptr<IRowSelector> rowSelector,
                     std::shared_ptr<DataManagerExtension> dataManager)
    : m_rowSelector(std::move(rowSelector)),
      m_dataManager(std::move(dataManager)) {
    if (!m_rowSelector) {
        throw std::invalid_argument("IRowSelector cannot be null");
    }
    if (!m_dataManager) {
        throw std::invalid_argument("DataManagerExtension cannot be null");
    }
}

TableView::TableView(TableView && other) noexcept
    : m_rowSelector(std::move(other.m_rowSelector)),
      m_dataManager(std::move(other.m_dataManager)),
      m_columns(std::move(other.m_columns)),
      m_colNameToIndex(std::move(other.m_colNameToIndex)),
      m_planCache(std::move(other.m_planCache)) {}

TableView & TableView::operator=(TableView && other) {
    if (this != &other) {
        m_rowSelector = std::move(other.m_rowSelector);
        m_dataManager = std::move(other.m_dataManager);
        m_columns = std::move(other.m_columns);
        m_colNameToIndex = std::move(other.m_colNameToIndex);
        m_planCache = std::move(other.m_planCache);
    }
    return *this;
}

size_t TableView::getRowCount() const {
    // Prefer expanded row count if any execution plan has entity-expanded rows cached
    for (auto const & entry : m_planCache) {
        if (!entry.second.getRows().empty()) {
            return entry.second.getRows().size();
        }
    }
    // If nothing cached yet, proactively attempt expansion using a line-source dependent column
    // Find a column whose source is an ILineSource
    for (auto const & column : m_columns) {
        try {
            auto const & dep = column->getSourceDependency();
            // Query line source without mutating the cache in a surprising way
            auto lineSource = m_dataManager->getLineSource(dep);
            if (lineSource) {
                // Trigger plan generation for this source to populate expansion rows
                auto & self = const_cast<TableView &>(*this);
                ExecutionPlan const & plan = self.getExecutionPlanFor(dep);
                if (!plan.getRows().empty()) {
                    return plan.getRows().size();
                }
                break; // only expand based on the first line source column
            }
        } catch (...) {
            // Ignore and continue
        }
    }
    return m_rowSelector->getRowCount();
}

size_t TableView::getColumnCount() const {
    return m_columns.size();
}

std::vector<std::string> TableView::getColumnNames() const {
    std::vector<std::string> names;
    names.reserve(m_columns.size());

    for (auto const & column: m_columns) {
        names.push_back(column->getName());
    }

    return names;
}

bool TableView::hasColumn(std::string const & name) const {
    return m_colNameToIndex.find(name) != m_colNameToIndex.end();
}

std::type_info const & TableView::getColumnType(std::string const & name) const {
    auto it = m_colNameToIndex.find(name);
    if (it == m_colNameToIndex.end()) {
        throw std::runtime_error("Column '" + name + "' not found in table");
    }
    
    return m_columns[it->second]->getType();
}

std::type_index TableView::getColumnTypeIndex(std::string const & name) const {
    return std::type_index(getColumnType(name));
}

ColumnDataVariant TableView::getColumnDataVariant(std::string const & name) {
    auto type_index = getColumnTypeIndex(name);
    
    // Dispatch from element type_index to vector type using template metaprogramming
    std::optional<ColumnDataVariant> result;
    
    for_each_type<SupportedColumnElementTypes>([&](auto, auto type_instance) {
        using ElementType = std::decay_t<decltype(type_instance)>;
        
        if (!result.has_value() && type_index == std::type_index(typeid(ElementType))) {
            // Found matching element type, get the vector data
            auto vectorData = getColumnValues<ElementType>(name);
            result = vectorData;  // This will construct the variant with the correct vector type
        }
    });

    if (!result.has_value()) {
        throw std::runtime_error("Unsupported column type: " + std::string(type_index.name()) + 
                                " for column: " + name);
    }
    
    return result.value();
}


void TableView::materializeAll() {
    std::set<std::string> materializing;

    for (auto const & column: m_columns) {
        if (!column->isMaterialized()) {
            materializeColumn(column->getName(), materializing);
        }
    }
}

void TableView::clearCache() {
    // Clear column caches
    for (auto & column: m_columns) {
        column->clearCache();
    }

    // Clear execution plan cache
    m_planCache.clear();
}

ExecutionPlan const & TableView::getExecutionPlanFor(std::string const & sourceName) {
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

    std::string const & name = column->getName();

    // Check for duplicate names
    if (hasColumn(name)) {
        throw std::runtime_error("Column '" + name + "' already exists");
    }

    // Add to collections
    size_t index = m_columns.size();
    m_columns.push_back(std::move(column));
    m_colNameToIndex[name] = index;
}

void TableView::materializeColumn(std::string const & columnName, std::set<std::string> & materializing) {
    // Check for circular dependencies
    if (materializing.find(columnName) != materializing.end()) {
        throw std::runtime_error("Circular dependency detected involving column: " + columnName);
    }

    // Check if column exists
    if (!hasColumn(columnName)) {
        throw std::runtime_error("Column '" + columnName + "' not found");
    }

    auto & column = m_columns[m_colNameToIndex[columnName]];

    // If already materialized, nothing to do
    if (column->isMaterialized()) {
        return;
    }

    // Mark as being materialized
    materializing.insert(columnName);

    // Materialize dependencies first
    auto const dependencies = column->getDependencies();
    for (auto const & dependency: dependencies) {
        if (hasColumn(dependency)) {
            materializeColumn(dependency, materializing);
        }
    }

    // Materialize this column
    column->materialize(this);// Use the IColumn interface method

    // Remove from materializing set
    materializing.erase(columnName);
}

ExecutionPlan TableView::generateExecutionPlan(std::string const & sourceName) {
    // Try to get the data source to understand its structure
    // First try as analog source

    

    auto analogSource = m_dataManager->getAnalogSource(sourceName);
    if (analogSource) {
        // Generate plan based on row selector type for analog data
        if (auto intervalSelector = dynamic_cast<IntervalSelector *>(m_rowSelector.get())) {
            auto const & intervals = intervalSelector->getIntervals();
            auto timeFrame = intervalSelector->getTimeFrame();
            return ExecutionPlan(intervals, timeFrame);
        }

        if (auto timestampSelector = dynamic_cast<TimestampSelector *>(m_rowSelector.get())) {
            auto const & indices = timestampSelector->getTimestamps();
            auto timeFrame = timestampSelector->getTimeFrame();
            return ExecutionPlan(indices, timeFrame);
        }

        if (auto indexSelector = dynamic_cast<IndexSelector *>(m_rowSelector.get())) {
            auto const & indices = indexSelector->getIndices();
            std::vector<TimeFrameIndex> timeFrameIndices;
            timeFrameIndices.reserve(indices.size());

            for (size_t index: indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }

            std::cout << "WARNING: IndexSelector is not supported for analog data" << std::endl;
            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    }

    // Try as interval source
    auto intervalSource = m_dataManager->getIntervalSource(sourceName);
    if (intervalSource) {
        // Generate plan based on row selector type for interval data
        if (auto intervalSelector = dynamic_cast<IntervalSelector *>(m_rowSelector.get())) {
            auto const & intervals = intervalSelector->getIntervals();
            auto timeFrame = intervalSelector->getTimeFrame();
            return ExecutionPlan(intervals, timeFrame);
        }

        if (auto timestampSelector = dynamic_cast<TimestampSelector *>(m_rowSelector.get())) {
            auto const & indices = timestampSelector->getTimestamps();
            auto timeFrame = timestampSelector->getTimeFrame();
            return ExecutionPlan(indices, timeFrame);
        }

        if (auto indexSelector = dynamic_cast<IndexSelector *>(m_rowSelector.get())) {
            auto const & indices = indexSelector->getIndices();
            std::vector<TimeFrameIndex> timeFrameIndices;
            timeFrameIndices.reserve(indices.size());

            for (size_t index: indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }

            std::cout << "WARNING: IndexSelector is not supported for interval data" << std::endl;
            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    }

    // Try as event source
    auto eventSource = m_dataManager->getEventSource(sourceName);
    if (eventSource) {
        // Generate plan based on row selector type for event data
        if (auto intervalSelector = dynamic_cast<IntervalSelector *>(m_rowSelector.get())) {
            auto const & intervals = intervalSelector->getIntervals();
            auto timeFrame = intervalSelector->getTimeFrame();
            return ExecutionPlan(intervals, timeFrame);
        }

        if (auto timestampSelector = dynamic_cast<TimestampSelector *>(m_rowSelector.get())) {
            auto const & indices = timestampSelector->getTimestamps();
            auto timeFrame = timestampSelector->getTimeFrame();
            return ExecutionPlan(indices, timeFrame);
        }

        if (auto indexSelector = dynamic_cast<IndexSelector *>(m_rowSelector.get())) {
            auto const & indices = indexSelector->getIndices();
            std::vector<TimeFrameIndex> timeFrameIndices;
            timeFrameIndices.reserve(indices.size());

            for (size_t index: indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }

            std::cout << "WARNING: IndexSelector is not supported for event data" << std::endl;

            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    }

    // Try as line source
    auto lineSource = m_dataManager->getLineSource(sourceName);
    if (lineSource) {
        // Default-on entity expansion for TimestampSelector
        if (auto timestampSelector = dynamic_cast<TimestampSelector *>(m_rowSelector.get())) {
            auto const & timestamps = timestampSelector->getTimestamps();
            auto timeFrame = timestampSelector->getTimeFrame();

            ExecutionPlan plan(std::vector<TimeFrameIndex>{}, timeFrame);
            // Build expanded rows: one row per line at that timestamp; drop timestamps with zero lines
            std::vector<RowId> rows;
            rows.reserve(timestamps.size());
            std::map<TimeFrameIndex, std::pair<size_t,size_t>> spans;

            // Determine if table contains any non-line columns; if so, we include singleton rows
            bool anyNonLineColumn = false;
            for (auto const & col : m_columns) {
                try {
                    auto const & dep = col->getSourceDependency();
                    if (!m_dataManager->getLineSource(dep)) {
                        anyNonLineColumn = true;
                        break;
                    }
                } catch (...) {
                    // Ignore
                }
            }

            size_t cursor = 0;
            for (auto const & t : timestamps) {
                auto const count = lineSource->getEntityCountAt(t);
                if (count == 0) {
                    if (anyNonLineColumn) {
                        spans.emplace(t, std::make_pair(cursor, static_cast<size_t>(1)));
                        rows.push_back(RowId{t, std::nullopt});
                        ++cursor;
                    }
                } else {
                    spans.emplace(t, std::make_pair(cursor, static_cast<size_t>(count)));
                    for (size_t i = 0; i < count; ++i) {
                        rows.push_back(RowId{t, static_cast<int>(i)});
                        ++cursor;
                    }
                }
            }

            plan.setRows(std::move(rows));
            plan.setTimeToRowSpan(std::move(spans));
            plan.setSourceId(DataSourceNameInterner::instance().intern(lineSource->getName()));
            plan.setSourceKind(ExecutionPlan::DataSourceKind::Line);
            return plan;
        }

        // IntervalSelector: keep legacy behavior (no expansion) for now
        if (auto intervalSelector = dynamic_cast<IntervalSelector *>(m_rowSelector.get())) {
            auto const & intervals = intervalSelector->getIntervals();
            auto timeFrame = intervalSelector->getTimeFrame();
            return ExecutionPlan(intervals, timeFrame);
        }

        if (auto indexSelector = dynamic_cast<IndexSelector *>(m_rowSelector.get())) {
            auto const & indices = indexSelector->getIndices();
            std::vector<TimeFrameIndex> timeFrameIndices;
            timeFrameIndices.reserve(indices.size());
            for (size_t index: indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }
            std::cout << "WARNING: IndexSelector is not supported for line data" << std::endl;
            auto plan = ExecutionPlan(std::move(timeFrameIndices), nullptr);
            plan.setSourceId(DataSourceNameInterner::instance().intern(lineSource->getName()));
            plan.setSourceKind(ExecutionPlan::DataSourceKind::Line);
            return plan;
        }
    }

    // Generic fallback: generate plan solely based on row selector when the source is unknown
    if (auto intervalSelector = dynamic_cast<IntervalSelector *>(m_rowSelector.get())) {
        auto const & intervals = intervalSelector->getIntervals();
        auto timeFrame = intervalSelector->getTimeFrame();
        std::cout << "WARNING: Data source '" << sourceName
                  << "' not found. Generating plan from IntervalSelector only." << std::endl;
        return ExecutionPlan(intervals, timeFrame);
    }

    if (auto timestampSelector = dynamic_cast<TimestampSelector *>(m_rowSelector.get())) {
        auto const & indices = timestampSelector->getTimestamps();
        auto timeFrame = timestampSelector->getTimeFrame();
        std::cout << "WARNING: Data source '" << sourceName
                  << "' not found. Generating plan from TimestampSelector only." << std::endl;
        return ExecutionPlan(indices, timeFrame);
    }

    if (auto indexSelector = dynamic_cast<IndexSelector *>(m_rowSelector.get())) {
        auto const & indices = indexSelector->getIndices();
        std::vector<TimeFrameIndex> timeFrameIndices;
        timeFrameIndices.reserve(indices.size());
        for (size_t index: indices) {
            timeFrameIndices.emplace_back(static_cast<int64_t>(index));
        }
        std::cout << "WARNING: Data source '" << sourceName
                  << "' not found. Generating plan from IndexSelector only." << std::endl;
        return ExecutionPlan(std::move(timeFrameIndices), nullptr);
    }

    throw std::runtime_error("Data source '" + sourceName + "' not found as analog, interval, event, or line source");
}

RowDescriptor TableView::getRowDescriptor(size_t row_index) const {
    if (m_rowSelector) {
        return m_rowSelector->getDescriptor(row_index);
    }
    return std::monostate{};
}

std::vector<EntityId> TableView::getRowEntityIds(size_t row_index) const {
    // Prefer entity-expanded plans if present
    for (auto const & [name, plan] : m_planCache) {
        (void)name;
        auto const & rows = plan.getRows();
        if (rows.empty() || row_index >= rows.size()) continue;
        auto const & row = rows[row_index];
        if (!row.entityIndex.has_value()) continue;

        // Resolve by source kind
        auto const sourceName = DataSourceNameInterner::instance().nameOf(plan.getSourceId());
        switch (plan.getSourceKind()) {
            case ExecutionPlan::DataSourceKind::Line: {
                auto lineSource = m_dataManager->getLineSource(sourceName);
                if (!lineSource) break;
                EntityId id = lineSource->getEntityIdAt(row.timeIndex, *row.entityIndex);
                if (id != 0) return {id};
                break;
            }
            case ExecutionPlan::DataSourceKind::Event: {
                auto eventSource = m_dataManager->getEventSource(sourceName);
                if (!eventSource) break;
                // For events we typically do not expand per-entity rows yet; fall through
                break;
            }
            case ExecutionPlan::DataSourceKind::Interval: {
                auto intervalSource = m_dataManager->getIntervalSource(sourceName);
                if (!intervalSource) break;
                // Interval entity expansion not yet implemented here
                break;
            }
            default: break;
        }
    }
    return {};
}

std::unique_ptr<IRowSelector> TableView::cloneRowSelectorFiltered(std::vector<size_t> const & keep_indices) const {
    if (!m_rowSelector) {
        return nullptr;
    }

    // IndexSelector
    if (auto indexSelector = dynamic_cast<IndexSelector const *>(m_rowSelector.get())) {
        std::vector<size_t> const & indices = indexSelector->getIndices();
        std::vector<size_t> filtered;
        filtered.reserve(keep_indices.size());
        for (size_t k : keep_indices) {
            if (k < indices.size()) {
                filtered.push_back(indices[k]);
            }
        }
        return std::make_unique<IndexSelector>(std::move(filtered));
    }

    // TimestampSelector
    if (auto timestampSelector = dynamic_cast<TimestampSelector const *>(m_rowSelector.get())) {
        auto const & timestamps = timestampSelector->getTimestamps();
        auto timeFrame = timestampSelector->getTimeFrame();
        std::vector<TimeFrameIndex> filtered;
        filtered.reserve(keep_indices.size());
        for (size_t k : keep_indices) {
            if (k < timestamps.size()) {
                filtered.push_back(timestamps[k]);
            }
        }
        return std::make_unique<TimestampSelector>(std::move(filtered), timeFrame);
    }

    // IntervalSelector
    if (auto intervalSelector = dynamic_cast<IntervalSelector const *>(m_rowSelector.get())) {
        auto const & intervals = intervalSelector->getIntervals();
        auto timeFrame = intervalSelector->getTimeFrame();
        std::vector<TimeFrameInterval> filtered;
        filtered.reserve(keep_indices.size());
        for (size_t k : keep_indices) {
            if (k < intervals.size()) {
                filtered.push_back(intervals[k]);
            }
        }
        return std::make_unique<IntervalSelector>(std::move(filtered), timeFrame);
    }

    // Fallback: preserve by indices if unknown type
    std::vector<size_t> identity;
    identity.reserve(keep_indices.size());
    for (size_t k : keep_indices) identity.push_back(k);
    return std::make_unique<IndexSelector>(std::move(identity));
}
