#ifndef EXECUTION_PLAN_H
#define EXECUTION_PLAN_H

#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/core/DataSourceNameInterner.hpp"
#include "utils/TableView/core/RowDescriptor.h"

#include <utility>
#include <vector>
#include <map>

/**
 * @brief Holds a cached, reusable access pattern for a specific data source.
 * 
 * This class holds the result of an expensive intermediate calculation,
 * typically the mapping of row definitions to specific data array indices.
 * It serves as a cache for computations that can be shared between columns.
 */
class ExecutionPlan {
public:
    enum class DataSourceKind : std::uint8_t { Unknown=0, Analog, Event, IntervalKind, Line };
    /**
     * @brief Default constructor.
     */
    ExecutionPlan() = default;

    /**
     * @brief Constructs an ExecutionPlan with indices.
     * @param indices Vector of TimeFrameIndex objects for direct access.
     * @param timeFrame Shared pointer to the TimeFrame associated with TimeFrameIndexes.
     */
    explicit ExecutionPlan(std::vector<TimeFrameIndex> indices, std::shared_ptr<TimeFrame> timeFrame);

    /**
     * @brief Constructs an ExecutionPlan with interval pairs.
     * @param intervals Vector of TimeFrameInterval objects for interval operations.
     * @param timeFrame Shared pointer to the TimeFrame associated with intervals.
     */
    explicit ExecutionPlan(std::vector<TimeFrameInterval> intervals, std::shared_ptr<TimeFrame> timeFrame);

    /**
     * @brief Gets the indices for direct access operations.
     * @return Const reference to the indices vector.
     */
    std::vector<TimeFrameIndex> const & getIndices() const;

    /**
     * @brief Gets the intervals for interval-based operations.
     * @return Const reference to the intervals vector.
     */
    std::vector<TimeFrameInterval> const & getIntervals() const;

    /**
     * @brief Checks if the plan contains indices.
     * @return True if indices are available, false otherwise.
     */
    bool hasIndices() const;

    /**
     * @brief Checks if the plan contains intervals.
     * @return True if intervals are available, false otherwise.
     */
    bool hasIntervals() const;

    /**
     * @brief Sets the indices for the execution plan.
     * @param indices Vector of TimeFrameIndex objects.
     */
    void setIndices(std::vector<TimeFrameIndex> indices);

    /**
     * @brief Sets the intervals for the execution plan.
     * @param intervals Vector of TimeFrameInterval objects.
     */
    void setIntervals(std::vector<TimeFrameInterval> intervals);

    /**
     * @brief Gets the TimeFrame associated with this execution plan.
     * @return Shared pointer to the TimeFrame.
     */
    std::shared_ptr<TimeFrame> getTimeFrame() const {
        return m_timeFrame;
    }

    // Entity-expanded API
    void setRows(std::vector<RowId> rows) {
        m_rows = std::move(rows);
    }

    std::vector<RowId> const& getRows() const {
        return m_rows;
    }

    bool hasEntities() const {
        if (m_rows.empty()) return false;
        for (auto const& r : m_rows) {
            if (r.entityIndex.has_value()) return true;
        }
        return false;
    }

    void setSourceId(DataSourceId id) { m_sourceId = id; }
    DataSourceId getSourceId() const { return m_sourceId; }

    void setSourceKind(DataSourceKind kind) { m_sourceKind = kind; }
    DataSourceKind getSourceKind() const { return m_sourceKind; }

    // Group spans per timestamp for fast broadcast
    void setTimeToRowSpan(std::map<TimeFrameIndex, std::pair<size_t,size_t>> map) {
        m_timeToRowSpan = std::move(map);
    }

    std::map<TimeFrameIndex, std::pair<size_t,size_t>> const& getTimeToRowSpan() const {
        return m_timeToRowSpan;
    }

    template<typename F>
    void forEachTimestampGroup(F&& f) const {
        for (auto const& [t, span] : m_timeToRowSpan) {
            f(t, span.first, span.second);
        }
    }

private:
    std::vector<TimeFrameIndex> m_indices;
    std::vector<TimeFrameInterval> m_intervals;
    std::shared_ptr<TimeFrame> m_timeFrame;
    // Extended entity-aware plan
    DataSourceId m_sourceId{0};
    DataSourceKind m_sourceKind{DataSourceKind::Unknown};
    std::vector<RowId> m_rows;
    std::map<TimeFrameIndex, std::pair<size_t,size_t>> m_timeToRowSpan;
};

#endif// EXECUTION_PLAN_H
