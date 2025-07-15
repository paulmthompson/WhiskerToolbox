#ifndef STANDARDIZE_COMPUTER_H
#define STANDARDIZE_COMPUTER_H

#include "../interfaces/IColumnComputer.h"
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>

/**
 * @brief Computer that standardizes (Z-scores) a numerical column.
 * 
 * This computer takes a single column name as a dependency and produces
 * a standardized version where each value is transformed to:
 * z = (x - mean) / stddev
 * 
 * The computer calculates the mean and standard deviation of the entire
 * source column, then applies the standardization formula element-wise.
 * 
 * This computer demonstrates column-to-column dependency handling, where
 * the output depends on the materialized data of another column rather
 * than directly on a data source.
 * 
 * @tparam T The input column type (typically double or float)
 */
template<typename T>
class StandardizeComputer : public IColumnComputer<double> {
public:
    /**
     * @brief Constructor.
     * @param dependency_col_name Name of the column to standardize.
     */
    explicit StandardizeComputer(std::string dependency_col_name);

    /**
     * @brief Returns the dependencies of this computer.
     * @return Vector containing the single dependency column name.
     */
    std::vector<std::string> getDependencies() const override;

    /**
     * @brief Returns the source dependency (empty for dependency-based computers).
     * @return Empty string since this computer depends on other columns, not data sources.
     */
    std::string getSourceDependency() const override;

    /**
     * @brief Computes the standardized values.
     * 
     * Note: This method currently serves as a placeholder. The actual computation
     * will be handled by the TableView system calling computeFromData with the
     * materialized dependency data.
     * 
     * @param plan The execution plan (unused for dependency-based computers).
     * @return Vector of standardized double values.
     */
    std::vector<double> compute(const ExecutionPlan& plan) const override;

    /**
     * @brief Computes standardized values from source data.
     * 
     * This method takes the source data directly and performs standardization:
     * 1. Calculates the mean of all values
     * 2. Calculates the standard deviation
     * 3. Applies z = (x - mean) / stddev to each value
     * 4. Handles edge cases (empty data, zero standard deviation)
     * 
     * @param source_values The values from the dependency column.
     * @return Vector of standardized double values.
     */
    std::vector<double> computeFromData(const std::vector<T>& source_values) const;

private:
    std::string m_dependency;
};

// Template method implementations
template<typename T>
StandardizeComputer<T>::StandardizeComputer(std::string dependency_col_name)
    : m_dependency(std::move(dependency_col_name)) {}

template<typename T>
std::vector<std::string> StandardizeComputer<T>::getDependencies() const {
    return {m_dependency};
}

template<typename T>
std::string StandardizeComputer<T>::getSourceDependency() const {
    return ""; // No data source dependency, only column dependency
}

template<typename T>
std::vector<double> StandardizeComputer<T>::compute(const ExecutionPlan& plan) const {
    // This method is a placeholder for the current interface
    // The actual computation will be done via computeFromData when the
    // TableView system is extended to handle column dependencies
    throw std::runtime_error("StandardizeComputer::compute should not be called directly. "
                            "This computer requires column dependency support in TableView.");
}

template<typename T>
std::vector<double> StandardizeComputer<T>::computeFromData(const std::vector<T>& source_values) const {
    if (source_values.empty()) {
        return {};
    }

    // Calculate mean
    double sum = std::accumulate(source_values.begin(), source_values.end(), 0.0);
    double mean = sum / source_values.size();
    
    // Calculate standard deviation
    // Using population standard deviation: sqrt(E[(X - μ)²])
    double sum_squared_diff = 0.0;
    for (const auto& val : source_values) {
        double diff = static_cast<double>(val) - mean;
        sum_squared_diff += diff * diff;
    }
    
    double variance = sum_squared_diff / source_values.size();
    double stddev = std::sqrt(variance);

    // Handle case of zero standard deviation to avoid division by zero
    if (stddev < 1e-9) {
        return std::vector<double>(source_values.size(), 0.0);
    }

    // Apply the standardization formula to each element
    std::vector<double> results;
    results.reserve(source_values.size());
    for (const auto& val : source_values) {
        results.push_back((static_cast<double>(val) - mean) / stddev);
    }

    return results;
}

#endif // STANDARDIZE_COMPUTER_H
