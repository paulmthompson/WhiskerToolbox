#ifndef ANALOG_DATA_ADAPTER_H
#define ANALOG_DATA_ADAPTER_H

#include "utils/TableView/interfaces/IAnalogSource.h"

#include <memory>
#include <vector>

class AnalogTimeSeries;

/**
 * @brief Adapter that exposes AnalogTimeSeries as an IAnalogSource.
 * 
 * This adapter provides a bridge between the existing AnalogTimeSeries class
 * and the IAnalogSource interface required by the TableView system.
 */
class AnalogDataAdapter : public IAnalogSource {
public:
    /**
     * @brief Constructs an AnalogDataAdapter.
     * 
     * 
     * 
     * @param analogData Shared pointer to the AnalogTimeSeries source.
     * @param timeFrame Shared pointer to the TimeFrame this data belongs to.
     * @param name The name of this data source.
     */
    AnalogDataAdapter(std::shared_ptr<AnalogTimeSeries> analogData,
                      std::shared_ptr<TimeFrame> timeFrame,
                      std::string name);

    // IAnalogSource interface implementation

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
     * @brief Gets the total number of samples in the source.
     * @return The number of samples.
     */
    [[nodiscard]] auto size() const -> size_t override;

    /**
     * @brief Gets the data within a specific time range.
     * 
     * This gets the data in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of floats representing the data in the specified range.
     */
    std::vector<float> getDataInRange(TimeFrameIndex start,
                                      TimeFrameIndex end,
                                      TimeFrame const * target_timeFrame) override;

private:
    /**
     * @brief Materializes the analog data if not already done.
     * 
     * Converts the float data from AnalogTimeSeries to double format
     * for the IAnalogSource interface.
     */
    void materializeData();

    std::shared_ptr<AnalogTimeSeries> m_analogData;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::string m_name;
    std::vector<double> m_materializedData;
    bool m_isMaterialized = false;
};

#endif// ANALOG_DATA_ADAPTER_H
