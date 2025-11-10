#ifndef LINE_LENGTH_COMPUTER_H
#define LINE_LENGTH_COMPUTER_H

#include "CoreGeometry/line_geometry.hpp"
#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IEntityProvider.h"
#include "utils/TableView/interfaces/ILineSource.h"

#include <memory>
#include <string>

/**
 * @brief Computer that calculates the cumulative length of line data.
 *
 * Source type: ILineSource
 * Selector type: Timestamp
 * Output type: float
 * 
 * Given a line source and a Timestamp-based ExecutionPlan, calculates the
 * cumulative length of each line. Multiple lines at the same timestamp are
 * expanded into separate rows, each with the length of the corresponding line.
 */
class LineLengthComputer : public IColumnComputer<float> {
public:
    LineLengthComputer(std::shared_ptr<ILineSource> lineSource,
                       std::string sourceName,
                       std::shared_ptr<TimeFrame> sourceTimeFrame)
        : m_lineSource(std::move(lineSource)),
          m_sourceName(std::move(sourceName)),
          m_sourceTimeFrame(std::move(sourceTimeFrame)) {}

    [[nodiscard]] std::pair<std::vector<float>, ColumnEntityIds> compute(ExecutionPlan const & plan) const override {
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
        std::vector<float> results;
        results.reserve(rowCount);

        // Use the plan's timeframe (rows are expressed in this timeframe)
        auto targetTF = plan.getTimeFrame().get();
        std::vector<EntityId> entityIds;
        entityIds.reserve(rowCount);

        for (size_t r = 0; r < rowCount; ++r) {
            TimeFrameIndex const tfIndex = indices[r];
            auto const this_time_entityIds = m_lineSource->getEntityIdsAtTime(tfIndex, targetTF);

            // Check if lines exist at this timestamp
            if (this_time_entityIds.empty()) {
                // No lines at this timestamp, return 0.0f
                results.push_back(0.0f);
                entityIds.push_back(EntityId(0));
            } else {
                // Lines exist at this timestamp
                // Prefer direct entity access if entity index is present
                if (entityIdx[r].has_value()) {
                    auto entity_index = *entityIdx[r];
                    entityIds.push_back(this_time_entityIds[entity_index]);

                    // Get the specific line at this entity index
                    auto const line = m_lineSource->getLineAt(tfIndex, entity_index);
                    if (line) {
                        results.push_back(calc_length(*line));
                    } else {
                        results.push_back(0.0f);
                    }
                } else {
                    // Use first entity if no specific entity index
                    entityIds.push_back(this_time_entityIds[0]);

                    // Get the first line at this timestamp
                    auto const line = m_lineSource->getLineAt(tfIndex, 0);
                    if (line) {
                        results.push_back(calc_length(*line));
                    } else {
                        results.push_back(0.0f);
                    }
                }
            }
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

#endif// LINE_LENGTH_COMPUTER_H
