#ifndef ANALOG_DATA_ADAPTER_H
#define ANALOG_DATA_ADAPTER_H

#include "utils/TableView/interfaces/IAnalogSource.h"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <memory>
#include <vector>

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
     * @param analogData Shared pointer to the AnalogTimeSeries source.
     * @param timeFrameId The TimeFrame ID this data belongs to.
     * @param name The name of this data source.
     */
    AnalogDataAdapter(std::shared_ptr<AnalogTimeSeries> analogData, int timeFrameId, std::string name);

    // IAnalogSource interface implementation
    [[nodiscard]] auto getName() const -> const std::string& override;
    [[nodiscard]] auto getTimeFrameId() const -> int override;
    [[nodiscard]] auto size() const -> size_t override;
    auto getDataSpan() -> std::span<const double> override;

    std::vector<float> getDataInRange(TimeFrameIndex start,
                                       TimeFrameIndex end,
                                       TimeFrame const & source_timeFrame,
                                       TimeFrame const & target_timeFrame) override;

private:
    /**
     * @brief Materializes the analog data if not already done.
     * 
     * Converts the float data from AnalogTimeSeries to double format
     * for the IAnalogSource interface.
     */
    void materializeData();

    std::shared_ptr<AnalogTimeSeries> m_analogData;
    int m_timeFrameId;
    std::string m_name;
    std::vector<double> m_materializedData;
    bool m_isMaterialized = false;
};

#endif // ANALOG_DATA_ADAPTER_H
