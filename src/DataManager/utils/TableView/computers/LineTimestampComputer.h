#ifndef LINE_TIMESTAMP_COMPUTER_H
#define LINE_TIMESTAMP_COMPUTER_H

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IEntityProvider.h"
#include "utils/TableView/interfaces/ILineSource.h"

#include <memory>
#include <string>

/**
 * @brief Computer that extracts timestamps from line data.
 *
 * Source type: ILineSource
 * Selector type: Timestamp
 * Output type: int64_t
 * 
 * Given a line source and a Timestamp-based ExecutionPlan, extracts the timestamp
 * value for each line. Multiple lines at the same timestamp are expanded into
 * separate rows, each with the same timestamp value.
 */
class LineTimestampComputer : public IColumnComputer<int64_t> {
public:
    LineTimestampComputer(std::shared_ptr<ILineSource> lineSource,
                          std::string sourceName,
                          std::shared_ptr<TimeFrame> sourceTimeFrame)
        : m_lineSource(std::move(lineSource)),
          m_sourceName(std::move(sourceName)),
          m_sourceTimeFrame(std::move(sourceTimeFrame)) {}

    [[nodiscard]] std::pair<std::vector<int64_t>, ColumnEntityIds> compute(ExecutionPlan const & plan) const override {
        // Determine rows: entity-expanded rows take precedence
        std::vector<TimeFrameIndex> indices;
        std::vector<std::optional<int>> entityIdx;
        if (!plan.getRows().empty()) {
            auto const & rows = plan.getRows();
            indices.reserve(rows.size());
            entityIdx.reserve(rows.size());
            for (auto const & r: rows) {
                indices.push_back(r.timeIndex);
                entityIdx.push_back(r.entityIndex);
            }
        } else if (plan.hasIndices()) {
            indices = plan.getIndices();
            entityIdx.resize(indices.size());
        } else {
            auto const & intervals = plan.getIntervals();
            indices.reserve(intervals.size());
            entityIdx.reserve(intervals.size());
            for (auto const & interval: intervals) {
                indices.push_back(interval.start);
                entityIdx.emplace_back(std::nullopt);
            }
        }

        size_t const rowCount = indices.size();
        std::vector<int64_t> results;
        results.reserve(rowCount);

        // Use the plan's timeframe (rows are expressed in this timeframe)
        auto targetTF = plan.getTimeFrame().get();
        std::vector<EntityId> entityIds;
        entityIds.reserve(rowCount);

        for (size_t r = 0; r < rowCount; ++r) {
            TimeFrameIndex const tfIndex = indices[r];
            auto const this_time_entityIds = m_lineSource->getEntityIdsAtTime(tfIndex, targetTF);

            // Prefer direct entity access if entity index is present
            if (entityIdx[r].has_value()) {
                entityIds.push_back(this_time_entityIds[*entityIdx[r]]);
            } else {
                // Use first entity if no specific entity index
                if (!this_time_entityIds.empty()) {
                    entityIds.push_back(this_time_entityIds[0]);
                } else {
                    entityIds.push_back(0);
                }
            }

            // Convert TimeFrameIndex to int64_t timestamp
            results.push_back(static_cast<int64_t>(tfIndex.getValue()));
        }

        return {results, entityIds};
    }

    [[nodiscard]] auto getDependencies() const -> std::vector<std::string> override {
        return {};
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

    [[nodiscard]] auto getEntityIdStructure() const -> EntityIdStructure override {
        return EntityIdStructure::Simple;
    }

private:
    std::shared_ptr<ILineSource> m_lineSource;
    std::string m_sourceName;
    std::shared_ptr<TimeFrame> m_sourceTimeFrame;
};

#endif// LINE_TIMESTAMP_COMPUTER_H
