#ifndef LINE_DATA_ADAPTER_H
#define LINE_DATA_ADAPTER_H

#include "utils/TableView/interfaces/ILineSource.h"

#include <memory>
#include <vector>

class LineData;

/**
 * @brief Adapter that exposes LineData as an ILineSource.
 * 
 * This adapter provides a bridge between the existing LineData class
 * and the ILineSource interface required by the TableView system.
 */
class LineDataAdapter : public ILineSource {
public:
    /**
     * @brief Constructs a LineDataAdapter.
     * @param lineData Shared pointer to the LineData source.
     * @param timeFrame Shared pointer to the TimeFrame this data belongs to.
     * @param name The name of this data source.
     */
    LineDataAdapter(std::shared_ptr<LineData> lineData,
                    std::shared_ptr<TimeFrame> timeFrame,
                    std::string name);

    // ILineSource interface implementation

    /**
     * @brief Gets the name of this data source.
     * 
     * This name is used for dependency tracking and ExecutionPlan caching
     * in the TableView system.
     * 
     * @return The name of the data source.
     */
    [[nodiscard]] auto getName() const -> std::string const & override;

    /**
     * @brief Gets the TimeFrame the data belongs to.
     * @return A shared pointer to the TimeFrame.
     */
    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override;

    /**
     * @brief Gets the total number of lines in the source.
     * @return The number of lines.
     */
    [[nodiscard]] auto size() const -> size_t override;

    /**
     * @brief Gets all lines in the source.
     * @return A vector of Line2D representing all lines in the source.
     */
    std::vector<Line2D> getLines() override;

    /**
     * @brief Gets the line data within a specific time range.
     * 
     * This gets the lines in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of Line2D representing the lines in the specified range.
     */
    std::vector<Line2D> getLinesInRange(TimeFrameIndex start,
                                        TimeFrameIndex end,
                                        TimeFrame const * target_timeFrame) override;

    /**
     * @brief Checks if this source has multiple samples (lines) at any timestamp.
     * 
     * @return True if any timestamp has more than one line, false otherwise.
     */
    bool hasMultiSamples() const override;

    // IEntityProvider implementation
    [[nodiscard]] auto getEntityCountAt(TimeFrameIndex t) const -> size_t override;
    [[nodiscard]] auto getLineAt(TimeFrameIndex t, int entityIndex) const -> Line2D const * override;

    // IEntityProvider addition: expose EntityId if present
    [[nodiscard]] EntityId getEntityIdAt(TimeFrameIndex t, int entityIndex) const override;

    [[nodiscard]] std::vector<EntityId> getEntityIdsAtTime(TimeFrameIndex t,
                                                           TimeFrame const * target_timeframe) const override;

private:
    std::shared_ptr<LineData> m_lineData;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::string m_name;
};

#endif// LINE_DATA_ADAPTER_H