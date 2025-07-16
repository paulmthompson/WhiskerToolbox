#ifndef ANALOG_SLICE_GATHERER_COMPUTER_H
#define ANALOG_SLICE_GATHERER_COMPUTER_H

#include "utils/TableView/columns/IColumn.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IAnalogSource.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

/**
 * @brief Computer for gathering analog data slices within intervals.
 * 
 * This computer strategy is responsible for iterating through an ExecutionPlan
 * of interval index pairs and, for each pair, copying the corresponding slice
 * of data from an IAnalogSource into a new vector. The result is a column where
 * each cell contains a std::vector<T> of the analog data within that interval.
 * 
 * @tparam T The numeric type for the gathered data (typically double or float).
 */
template<typename T = double>
class AnalogSliceGathererComputer : public IColumnComputer<std::vector<T>> {
public:
    /**
     * @brief Constructor for AnalogSliceGathererComputer.
     * @param source Shared pointer to the analog source to gather data from.
     */
    explicit AnalogSliceGathererComputer(std::shared_ptr<IAnalogSource> source)
        : m_source(std::move(source)) {
        if (!m_source) {
            throw std::invalid_argument("IAnalogSource cannot be null");
        }
    }

    /**
     * @brief Constructor with custom source name.
     * @param source Shared pointer to the analog source to gather data from.
     * @param sourceName Custom name for the source dependency.
     */
    AnalogSliceGathererComputer(std::shared_ptr<IAnalogSource> source, std::string sourceName)
        : m_source(std::move(source)),
          m_sourceName(std::move(sourceName)) {
        if (!m_source) {
            throw std::invalid_argument("IAnalogSource cannot be null");
        }
    }

    /**
     * @brief Computes the gathered data slices for all intervals.
     * 
     * This method iterates through the ExecutionPlan's intervals and gathers
     * the corresponding data slices from the analog source.
     * 
     * @param plan The execution plan containing interval boundaries.
     * @return Vector of vectors, where each inner vector contains the data slice for one interval.
     */
    [[nodiscard]] auto compute(ExecutionPlan const & plan) const -> std::vector<std::vector<T>> override {
        if (!plan.hasIntervals()) {
            throw std::invalid_argument("ExecutionPlan must contain intervals for AnalogSliceGathererComputer");
        }

        // Get the list of intervals from the execution plan
        // This should also return the TimeFrame the intervals belong to
        // They way they can be converted.
        auto const & intervals = plan.getIntervals();
        auto destinationTimeFrame = plan.getTimeFrame();

        // This is our final result: a vector of vectors
        std::vector<std::vector<T>> results;
        results.reserve(intervals.size());

        for (auto const & interval: intervals) {
            
            auto sliceView = m_source->getDataInRange(
                interval.start, 
                interval.end, 
                destinationTimeFrame.get()
            );

            // This constructs the vector for the cell
            results.emplace_back(sliceView.begin(), sliceView.end());
        }
        return results;
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName.empty() ? m_source->getName() : m_sourceName;
    }

private:
    std::shared_ptr<IAnalogSource> m_source;
    std::string m_sourceName;// Optional custom source name
};

#endif// ANALOG_SLICE_GATHERER_COMPUTER_H
