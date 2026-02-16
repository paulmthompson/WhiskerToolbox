#ifndef DATA_MANAGER_FIXTURE_HPP
#define DATA_MANAGER_FIXTURE_HPP

#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "builders/TimeFrameBuilder.hpp"

#include <memory>
#include <vector>

/**
 * @brief Basic fixture for DataManager tests.
 *
 * Provides a DataManager instance and a standard TimeFrame (0, 10, 20, 30).
 */
class DataManagerFixture {
protected:
    DataManagerFixture() {
        m_data_manager = std::make_unique<DataManager>();

        // Setup a standard timeframe
        m_times = {0, 10, 20, 30};
        m_time_frame = TimeFrameBuilder()
            .withTimes(m_times)
            .build();

        // m_data_manager owns the timeframe via setTime logic (shared_ptr)
        m_data_manager->setTime(TimeKey("test_time"), m_time_frame);
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
    std::vector<int> m_times;
};

#endif // DATA_MANAGER_FIXTURE_HPP
