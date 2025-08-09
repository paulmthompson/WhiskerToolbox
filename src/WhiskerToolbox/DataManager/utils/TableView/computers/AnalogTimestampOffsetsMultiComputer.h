#ifndef ANALOG_TIMESTAMP_OFFSETS_MULTI_COMPUTER_H
#define ANALOG_TIMESTAMP_OFFSETS_MULTI_COMPUTER_H

#include "utils/TableView/interfaces/IMultiColumnComputer.h"
#include "utils/TableView/interfaces/IAnalogSource.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Multi-output computer that samples an analog source at timestamp offsets.
 *
 * Given a timestamp-based ExecutionPlan, this computer produces one output column per
 * configured offset. For each row timestamp t and for each integer offset o, the output
 * value is the analog sample at time index (t + o), with cross-timeframe conversion
 * handled by the analog source adapter.
 */
class AnalogTimestampOffsetsMultiComputer : public IMultiColumnComputer<double> {
public:
    /**
     * @brief Construct the computer with a source and explicit offsets.
     * @param source Shared pointer to the analog source.
     * @param sourceName Name to use for dependency lookup/caching.
     * @param offsets Integer offsets relative to the row timestamps; one column per offset.
     */
    AnalogTimestampOffsetsMultiComputer(std::shared_ptr<IAnalogSource> source,
                                        std::string sourceName,
                                        std::vector<int> offsets)
        : m_source(std::move(source)),
          m_sourceName(std::move(sourceName)),
          m_offsets(std::move(offsets)) {}

    /**
     * @brief Compute all output columns for the provided plan.
     * @param plan Execution plan (Timestamp or entity-expanded rows supported).
     * @return Vector of columns; size equals number of offsets.
     */
    [[nodiscard]] auto computeBatch(ExecutionPlan const & plan) const -> std::vector<std::vector<double>> override;

    /**
     * @brief Suffix names for each offset output.
     * @return Vector like {".t-2", ".t-1", ".t+0", ".t+1", ...}
     */
    [[nodiscard]] auto getOutputNames() const -> std::vector<std::string> override;

    /**
     * @brief Source dependency name for this computation.
     */
    [[nodiscard]] auto getSourceDependency() const -> std::string override { return m_sourceName; }

    /**
     * @brief Additional column dependencies (none).
     */
    [[nodiscard]] auto getDependencies() const -> std::vector<std::string> override { return {}; }

private:
    std::shared_ptr<IAnalogSource> m_source;
    std::string m_sourceName;
    std::vector<int> m_offsets;
};

#endif // ANALOG_TIMESTAMP_OFFSETS_MULTI_COMPUTER_H


