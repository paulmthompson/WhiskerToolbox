#ifndef INTERVAL_PROPERTY_COMPUTER_H
#define INTERVAL_PROPERTY_COMPUTER_H

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/columns/IColumn.h"
#include "Entity/EntityTypes.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Enumeration of operations that can be performed on interval properties.
 */
enum class IntervalProperty : std::uint8_t {
    Start,     ///< Returns the start time/index of the interval
    End,       ///< Returns the end time/index of the interval
    Duration   ///< Returns the duration (end - start) of the interval
};

/**
 * @brief Templated computer for extracting properties from time intervals.
 * 
 * Source type: IIntervalSource
 * Selector type: Interval
 * Output type: T
 * 
 * This computer works with IIntervalSource data and can extract different properties
 * from intervals that are used as row selectors. The template parameter T
 * determines the return type based on the property being extracted:
 * - IntervalProperty::Start requires T = int64_t or float
 * - IntervalProperty::End requires T = int64_t or float
 * - IntervalProperty::Duration requires T = int64_t or float
 */
template<typename T>
class IntervalPropertyComputer : public IColumnComputer<T> {
public:
    /**
     * @brief Constructor for IntervalPropertyComputer.
     * @param source Shared pointer to the interval source.
     * @param property The property to extract from intervals.
     * @param sourceName The name of the data source (for dependency tracking).
     */
    IntervalPropertyComputer(std::shared_ptr<IIntervalSource> source, 
                            IntervalProperty property,
                            std::string sourceName)
        : m_source(std::move(source)), m_property(property), m_sourceName(std::move(sourceName)) {}

    /**
     * @brief Computes the result for all intervals in the execution plan.
     * @param plan The execution plan containing interval boundaries.
     * @return Vector of computed results for each interval.
     */
    [[nodiscard]] auto compute(const ExecutionPlan& plan) const -> std::vector<T> override {
        if (!plan.hasIntervals()) {
            throw std::runtime_error("IntervalPropertyComputer requires an ExecutionPlan with intervals");
        }
        
        auto intervals = plan.getIntervals();
        auto destinationTimeFrame = plan.getTimeFrame();
        
        std::vector<T> results;
        results.reserve(intervals.size());
        
        for (const auto& interval : intervals) {
            switch (m_property) {
                case IntervalProperty::Start:
                    results.push_back(static_cast<T>(interval.start.getValue()));
                    break;
                case IntervalProperty::End:
                    results.push_back(static_cast<T>(interval.end.getValue()));
                    break;
                case IntervalProperty::Duration:
                    results.push_back(static_cast<T>(interval.end.getValue() - interval.start.getValue()));
                    break;
                default:
                    throw std::runtime_error("Unknown IntervalProperty");
            }
        }
        
        return results;
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

    /**
     * @brief Gets the EntityID structure type for this computer.
     * @return EntityIdStructure::Simple since each interval maps to one value.
     */
    [[nodiscard]] EntityIdStructure getEntityIdStructure() const override {
        return EntityIdStructure::Simple;
    }

    /**
     * @brief Checks if this computer can provide EntityID information.
     * @return True since intervals have EntityIDs.
     */
    [[nodiscard]] bool hasEntityIds() const override {
        return true;
    }

    /**
     * @brief Computes all EntityIDs for the column using Simple structure.
     * @param plan The execution plan containing interval boundaries.
     * @return ColumnEntityIds variant containing std::vector<EntityId>.
     */
    [[nodiscard]] ColumnEntityIds computeColumnEntityIds(ExecutionPlan const & plan) const override {
        if (!plan.hasIntervals()) {
            return std::monostate{};
        }
        
        auto intervals = plan.getIntervals();
        std::vector<EntityId> entity_ids;
        entity_ids.reserve(intervals.size());
        
        // Extract EntityIDs from the interval source
        // The intervals in the ExecutionPlan correspond to the source intervals by index
        for (size_t i = 0; i < intervals.size(); ++i) {
            EntityId entity_id = m_source->getEntityIdAt(i);
            entity_ids.push_back(entity_id);
        }
        
        return entity_ids;
    }

private:
    std::shared_ptr<IIntervalSource> m_source;
    IntervalProperty m_property;
    std::string m_sourceName;
};



#endif // INTERVAL_PROPERTY_COMPUTER_H
