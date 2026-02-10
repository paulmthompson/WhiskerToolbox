#include "TableView.h"

#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/columns/Column.h"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "utils/TableView/interfaces/IAnalogSource.h"
#include "Points/Point_Data.hpp"
#include "utils/TableView/interfaces/IRowSelector.h"


#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <variant>

namespace {

// Utility to create an overloaded set for std::visit
template<class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

// Visitor over IRowSelector without exposing dynamic_casts at call sites
template<typename FInterval, typename FTimestamp, typename FIndex>
ExecutionPlan visitSelector(IRowSelector const & selector,
                            FInterval && onInterval,
                            FTimestamp && onTimestamp,
                            FIndex && onIndex) {
    if (auto s = dynamic_cast<IntervalSelector const *>(&selector)) {
        return onInterval(*s);
    }
    if (auto s = dynamic_cast<TimestampSelector const *>(&selector)) {
        return onTimestamp(*s);
    }
    if (auto s = dynamic_cast<IndexSelector const *>(&selector)) {
        return onIndex(*s);
    }
    throw std::runtime_error("Unknown IRowSelector concrete type");
}

// Analog source -> plan
ExecutionPlan makePlanFromAnalog(std::shared_ptr<IAnalogSource> const & /*analog*/,
                                 IRowSelector const & selector) {
    return visitSelector(
        selector,
        // Interval
        [&](IntervalSelector const & intervalSelector) {
            return ExecutionPlan(intervalSelector.getIntervals(), intervalSelector.getTimeFrame());
        },
        // Timestamp
        [&](TimestampSelector const & timestampSelector) {
            return ExecutionPlan(timestampSelector.getTimestamps(), timestampSelector.getTimeFrame());
        },
        // Index
        [&](IndexSelector const & indexSelector) {
            std::vector<TimeFrameIndex> timeFrameIndices;
            auto const & indices = indexSelector.getIndices();
            timeFrameIndices.reserve(indices.size());
            for (size_t index : indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }
            std::cout << "WARNING: IndexSelector is not supported for analog data" << std::endl;
            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    );
}

// Interval source -> plan
ExecutionPlan makePlanFromInterval(std::shared_ptr<DigitalIntervalSeries> const & /*interval*/,
                                   IRowSelector const & selector) {
    return visitSelector(
        selector,
        [&](IntervalSelector const & intervalSelector) {
            return ExecutionPlan(intervalSelector.getIntervals(), intervalSelector.getTimeFrame());
        },
        [&](TimestampSelector const & timestampSelector) {
            return ExecutionPlan(timestampSelector.getTimestamps(), timestampSelector.getTimeFrame());
        },
        [&](IndexSelector const & indexSelector) {
            std::vector<TimeFrameIndex> timeFrameIndices;
            auto const & indices = indexSelector.getIndices();
            timeFrameIndices.reserve(indices.size());
            for (size_t index : indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }
            std::cout << "WARNING: IndexSelector is not supported for interval data" << std::endl;
            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    );
}

// Event source -> plan
ExecutionPlan makePlanFromEvent(std::shared_ptr<DigitalEventSeries> const & /*event*/,
                                IRowSelector const & selector) {
    return visitSelector(
        selector,
        [&](IntervalSelector const & intervalSelector) {
            return ExecutionPlan(intervalSelector.getIntervals(), intervalSelector.getTimeFrame());
        },
        [&](TimestampSelector const & timestampSelector) {
            return ExecutionPlan(timestampSelector.getTimestamps(), timestampSelector.getTimeFrame());
        },
        [&](IndexSelector const & indexSelector) {
            std::vector<TimeFrameIndex> timeFrameIndices;
            auto const & indices = indexSelector.getIndices();
            timeFrameIndices.reserve(indices.size());
            for (size_t index : indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }
            std::cout << "WARNING: IndexSelector is not supported for event data" << std::endl;
            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    );
}

// Line source -> plan (needs columns and dm for entity expansion decision + ids)
ExecutionPlan makePlanFromLine(std::shared_ptr<LineData> const & lineSource,
                               IRowSelector const & selector,
                               std::vector<std::shared_ptr<IColumn>> const & columns,
                               DataManagerExtension & dm) {
    return visitSelector(
        selector,
        // IntervalSelector: legacy (no expansion)
        [&](IntervalSelector const & intervalSelector) {
            return ExecutionPlan(intervalSelector.getIntervals(), intervalSelector.getTimeFrame());
        },
        // TimestampSelector: entity expansion
        [&](TimestampSelector const & timestampSelector) {
            auto const & timestamps = timestampSelector.getTimestamps();
            auto timeFrame = timestampSelector.getTimeFrame();

            ExecutionPlan plan(std::vector<TimeFrameIndex>{}, timeFrame);

            // Determine if table contains any non-line columns; if so include singleton rows
            bool anyNonLineColumn = false;
            for (auto const & col : columns) {
                try {
                    auto const & dep = col->getSourceDependency();
                    auto dep_data = dm.resolveSource(dep);
                    if (!std::holds_alternative<std::shared_ptr<LineData>>(dep_data.value())) {
                        anyNonLineColumn = true;
                        break;
                    }
                } catch (...) {
                }
            }

            std::vector<RowId> rows;
            rows.reserve(timestamps.size());
            std::map<TimeFrameIndex, std::pair<size_t, size_t>> spans;

            size_t cursor = 0;
            for (auto const & t : timestamps) {
                auto lines_view = lineSource->getAtTime(t, *timeFrame);
                auto const count = std::ranges::distance(lines_view);
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
            // LineData doesn't store its name, so we'd need to pass it separately
            // For now, use empty string or a placeholder
            plan.setSourceId(0); // Will be set by caller if needed
            plan.setSourceKind(ExecutionPlan::DataSourceKind::Line);
            return plan;
        },
        // IndexSelector: unsupported
        [&](IndexSelector const & indexSelector) {
            std::vector<TimeFrameIndex> timeFrameIndices;
            auto const & indices = indexSelector.getIndices();
            timeFrameIndices.reserve(indices.size());
            for (size_t index : indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }
            std::cout << "WARNING: IndexSelector is not supported for line data" << std::endl;
            auto plan = ExecutionPlan(std::move(timeFrameIndices), nullptr);
            plan.setSourceId(0); // Will be set by caller if needed
            plan.setSourceKind(ExecutionPlan::DataSourceKind::Line);
            return plan;
        }
    );
}

// Point source -> plan (treat similar to event/interval/timestamp)
ExecutionPlan makePlanFromPoint(std::shared_ptr<PointData> const & /*point*/,
                                IRowSelector const & selector) {
    return visitSelector(
        selector,
        [&](IntervalSelector const & intervalSelector) {
            return ExecutionPlan(intervalSelector.getIntervals(), intervalSelector.getTimeFrame());
        },
        [&](TimestampSelector const & timestampSelector) {
            return ExecutionPlan(timestampSelector.getTimestamps(), timestampSelector.getTimeFrame());
        },
        [&](IndexSelector const & indexSelector) {
            std::vector<TimeFrameIndex> timeFrameIndices;
            auto const & indices = indexSelector.getIndices();
            timeFrameIndices.reserve(indices.size());
            for (size_t index : indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }
            std::cout << "WARNING: IndexSelector is not supported for point data" << std::endl;
            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    );
}

} // namespace

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
      m_planCache(std::move(other.m_planCache)),
      m_direct_entity_ids(std::move(other.m_direct_entity_ids)) {}

TableView & TableView::operator=(TableView && other) {
    if (this != &other) {
        m_rowSelector = std::move(other.m_rowSelector);
        m_dataManager = std::move(other.m_dataManager);
        m_columns = std::move(other.m_columns);
        m_colNameToIndex = std::move(other.m_colNameToIndex);
        m_planCache = std::move(other.m_planCache);
        m_direct_entity_ids = std::move(other.m_direct_entity_ids);
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
    // Resolve to a concrete source adapter once, then dispatch with std::visit
    auto resolved = m_dataManager->resolveSource(sourceName);
    if (resolved) {
        return std::visit(
            Overloaded{
                [&](std::shared_ptr<IAnalogSource> const & a) {
                    return makePlanFromAnalog(a, *m_rowSelector);
                },
                [&](std::shared_ptr<DigitalEventSeries> const & e) {
                    return makePlanFromEvent(e, *m_rowSelector);
                },
                [&](std::shared_ptr<DigitalIntervalSeries> const & i) {
                    return makePlanFromInterval(i, *m_rowSelector);
                },
                [&](std::shared_ptr<LineData> const & l) {
                    return makePlanFromLine(l, *m_rowSelector, m_columns, *m_dataManager);
                },
                [&](std::shared_ptr<PointData> const & p) {
                    return makePlanFromPoint(p, *m_rowSelector);
                }
            },
            *resolved
        );
    }

    // Fallback: generate plan solely from the selector if source is unknown
    return visitSelector(
        *m_rowSelector,
        [&](IntervalSelector const & intervalSelector) {
            auto const & intervals = intervalSelector.getIntervals();
            auto timeFrame = intervalSelector.getTimeFrame();
            std::cout << "WARNING: Data source '" << sourceName
                      << "' not found. Generating plan from IntervalSelector only." << std::endl;
            return ExecutionPlan(intervals, timeFrame);
        },
        [&](TimestampSelector const & timestampSelector) {
            auto const & indices = timestampSelector.getTimestamps();
            auto timeFrame = timestampSelector.getTimeFrame();
            std::cout << "WARNING: Data source '" << sourceName
                      << "' not found. Generating plan from TimestampSelector only." << std::endl;
            return ExecutionPlan(indices, timeFrame);
        },
        [&](IndexSelector const & indexSelector) {
            auto const & indices = indexSelector.getIndices();
            std::vector<TimeFrameIndex> timeFrameIndices;
            timeFrameIndices.reserve(indices.size());
            for (size_t index : indices) {
                timeFrameIndices.emplace_back(static_cast<int64_t>(index));
            }
            std::cout << "WARNING: Data source '" << sourceName
                      << "' not found. Generating plan from IndexSelector only." << std::endl;
            return ExecutionPlan(std::move(timeFrameIndices), nullptr);
        }
    );
}

TableViewRowDescriptor TableView::getRowDescriptor(size_t row_index) const {
    if (m_rowSelector) {
        return m_rowSelector->getDescriptor(row_index);
    }
    return std::monostate{};
}

std::vector<EntityId> TableView::getRowEntityIds(size_t row_index) const {
    // First check if we have direct EntityIds (for transformed tables)
    if (!m_direct_entity_ids.empty()) {
        if (row_index < m_direct_entity_ids.size()) {
            return m_direct_entity_ids[row_index];
        }
        return {};
    }
    
    // Final fallback: collect EntityIDs from all columns (for mixed/derived sources)
    std::set<EntityId> entity_set;
    for (auto const & column : m_columns) {
        //if (column->hasEntityIds()) {
            // Use the column's getCellEntityIds method
            auto cell_entities = column->getCellEntityIds(row_index);
            for (EntityId entity_id : cell_entities) {
                if (entity_id != EntityId(0)) {
                    entity_set.insert(entity_id);
                }
            }
        //}
    }
    
    // Return the unified set of EntityIDs for this row
    return std::vector<EntityId>(entity_set.begin(), entity_set.end());
}

bool TableView::hasEntityColumn() const {
    // Check if we have direct EntityIds first
    if (!m_direct_entity_ids.empty()) {
        return true;
    }
    
    // Fallback to execution plan-based check
    size_t row_count = getRowCount();
    if (row_count == 0) {
        return false;
    }
    
    // Check the first row to see if EntityIds are available
    auto entity_ids = getRowEntityIds(0);
    return !entity_ids.empty();
}

std::vector<std::vector<EntityId>> TableView::getEntityIds() const {
    // If we have direct EntityIds, return them
    if (!m_direct_entity_ids.empty()) {
        return m_direct_entity_ids;
    }
    
    // Fallback to execution plan-based EntityIds
    std::vector<std::vector<EntityId>> all_entity_ids;
    size_t row_count = getRowCount();
    
    for (size_t i = 0; i < row_count; ++i) {
        auto row_entity_ids = getRowEntityIds(i);
        if (!row_entity_ids.empty()) {
            all_entity_ids.push_back(std::move(row_entity_ids));
        } else {
            all_entity_ids.push_back({});
        }
    }
    
    return all_entity_ids;
}

void TableView::setDirectEntityIds(std::vector<std::vector<EntityId>> entity_ids) {
    m_direct_entity_ids = std::move(entity_ids);
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

bool TableView::hasColumnEntityIds(std::string const & columnName) const {
    auto it = m_colNameToIndex.find(columnName);
    if (it == m_colNameToIndex.end()) {
        return false;
    }
    
    size_t columnIndex = it->second;
    if (columnIndex >= m_columns.size()) {
        return false;
    }
    
    return m_columns[columnIndex]->hasEntityIds();
}

ColumnEntityIds TableView::getColumnEntityIds(std::string const & columnName) const {
    auto it = m_colNameToIndex.find(columnName);
    if (it == m_colNameToIndex.end()) {
        return {};
    }
    
    size_t columnIndex = it->second;
    if (columnIndex >= m_columns.size()) {
        return {};
    }
    
    return m_columns[columnIndex]->getColumnEntityIds();
}

std::vector<EntityId> TableView::getCellEntityIds(std::string const & columnName, size_t rowIndex) const {
    auto it = m_colNameToIndex.find(columnName);
    if (it == m_colNameToIndex.end()) {
        return {};
    }
    
    size_t columnIndex = it->second;
    if (columnIndex >= m_columns.size()) {
        return {};
    }
    
    return m_columns[columnIndex]->getCellEntityIds(rowIndex);
}
