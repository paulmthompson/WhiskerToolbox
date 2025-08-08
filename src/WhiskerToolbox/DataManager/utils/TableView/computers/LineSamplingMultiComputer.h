#ifndef LINE_SAMPLING_MULTI_COMPUTER_H
#define LINE_SAMPLING_MULTI_COMPUTER_H

#include "utils/TableView/interfaces/IMultiColumnComputer.h"
#include "utils/TableView/interfaces/ILineSource.h"
#include "CoreGeometry/line_geometry.hpp"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Multi-output computer that samples x and y at equally spaced positions along a line.
 *
 * Given a line source and a Timestamp-based ExecutionPlan, divides the [0,1] fractional
 * length into (segments) equal parts, yielding (segments+1) sample positions. For each
 * position, outputs two columns: x and y, in that order, resulting in 2*(segments+1)
 * outputs.
 */
class LineSamplingMultiComputer : public IMultiColumnComputer<double> {
public:
    LineSamplingMultiComputer(std::shared_ptr<ILineSource> lineSource,
                              std::string sourceName,
                              std::shared_ptr<TimeFrame> sourceTimeFrame,
                              int segments)
        : m_lineSource(std::move(lineSource)),
          m_sourceName(std::move(sourceName)),
          m_sourceTimeFrame(std::move(sourceTimeFrame)),
          m_segments(segments < 1 ? 1 : segments) {}

    [[nodiscard]] auto computeBatch(ExecutionPlan const & plan) const -> std::vector<std::vector<double>> override {
        // Determine rows: timestamps or intervals; we support timestamps for sampling
        std::vector<TimeFrameIndex> indices;
        if (plan.hasIndices()) {
            indices = plan.getIndices();
        } else {
            // For intervals, sample at start index; extend later if needed
            auto const & intervals = plan.getIntervals();
            indices.reserve(intervals.size());
            for (auto const & interval : intervals) {
                indices.push_back(interval.start);
            }
        }

        size_t const rowCount = indices.size();
        int const positions = m_segments + 1;
        int const outputs = positions * 2; // x then y per position

        std::vector<std::vector<double>> results(outputs);
        for (auto & vec : results) {
            vec.resize(rowCount);
        }

        // Precompute fractional positions
        std::vector<float> fractions;
        fractions.reserve(static_cast<size_t>(positions));
        for (int i = 0; i <= m_segments; ++i) {
            fractions.push_back(static_cast<float>(i) / static_cast<float>(m_segments));
        }

        // For each row, obtain the representative line and sample
        // Use the plan's timeframe (rows are expressed in this timeframe)
        auto targetTF = plan.getTimeFrame().get();

        for (size_t r = 0; r < rowCount; ++r) {
            TimeFrameIndex const tfIndex = indices[r];
            // Fetch lines at the given time in the target timeframe, converted to the source timeframe
            auto lines = m_lineSource->getLinesInRange(tfIndex, tfIndex, targetTF);
            if (lines.empty()) {
                // Fill zeros if no line
                for (int p = 0; p < positions; ++p) {
                    results[2 * p][r] = 0.0;
                    results[2 * p + 1][r] = 0.0;
                }
                continue;
            }

            // Use the first line at this time
            Line2D const & line = lines.front();
            for (int p = 0; p < positions; ++p) {
                auto optPoint = point_at_fractional_position(line, fractions[static_cast<size_t>(p)], true);
                if (optPoint.has_value()) {
                    results[2 * p][r] = static_cast<double>(optPoint->x);
                    results[2 * p + 1][r] = static_cast<double>(optPoint->y);
                } else {
                    results[2 * p][r] = 0.0;
                    results[2 * p + 1][r] = 0.0;
                }
            }
        }

        return results;
    }

    [[nodiscard]] auto getOutputNames() const -> std::vector<std::string> override {
        std::vector<std::string> suffixes;
        int const positions = m_segments + 1;
        suffixes.reserve(static_cast<size_t>(positions * 2));
        for (int i = 0; i <= m_segments; ++i) {
            double frac = static_cast<double>(i) / static_cast<double>(m_segments);
            // Fixed width to 3 decimals for readability
            char buf[32];
            std::snprintf(buf, sizeof(buf), "@%.3f", frac);
            suffixes.emplace_back(std::string{".x"} + buf);
            suffixes.emplace_back(std::string{".y"} + buf);
        }
        return suffixes;
    }

    [[nodiscard]] auto getDependencies() const -> std::vector<std::string> override {
        return {};
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

private:
    std::shared_ptr<ILineSource> m_lineSource;
    std::string m_sourceName;
    std::shared_ptr<TimeFrame> m_sourceTimeFrame;
    int m_segments;
};

#endif // LINE_SAMPLING_MULTI_COMPUTER_H


