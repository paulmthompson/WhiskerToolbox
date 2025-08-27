#ifndef TABLE_VIEW_BUILDER_H
#define TABLE_VIEW_BUILDER_H

#include "TableView.h"

#include "utils/TableView/columns/Column.h"
#include "utils/TableView/columns/IColumn.h"
#include "utils/TableView/interfaces/MultiComputerOutputView.hpp"

#include <memory>
#include <string>
#include <vector>

class DataManagerExtension;
template<typename T>
class IColumnComputer;
template<typename T>
class IMultiColumnComputer;
class IRowSelector;

/**
 * @brief Builder class for constructing TableView objects with a fluent API.
 * 
 * The TableViewBuilder provides a step-by-step, fluent API for constructing
 * TableView objects, simplifying the setup of complex configurations.
 */
class TableViewBuilder {
public:
    /**
     * @brief Constructs a TableViewBuilder with the given data manager.
     * @param dataManager Shared pointer to the data manager extension.
     */
    explicit TableViewBuilder(std::shared_ptr<DataManagerExtension> dataManager);

    ~TableViewBuilder();

    /**
     * @brief Sets the row selector that defines the table rows.
     * @param rowSelector Unique pointer to the row selector.
     * @return Reference to this builder for method chaining.
     */
    auto setRowSelector(std::unique_ptr<IRowSelector> rowSelector) -> TableViewBuilder &;

    /**
     * @brief Adds a column to the table being built.
     * @param name The name of the column.
     * @param computer Unique pointer to the column computer.
     * @return Reference to this builder for method chaining.
     */
    auto addColumn(std::string const & name, std::unique_ptr<IColumnComputer<double>> computer) -> TableViewBuilder &;

    /**
     * @brief Adds a templated column to the table being built.
     * @tparam T The type of the column data.
     * @param name The name of the column.
     * @param computer Unique pointer to the templated column computer.
     * @return Reference to this builder for method chaining.
     */
    template<typename T>
    auto addColumn(std::string const & name, std::unique_ptr<IColumnComputer<T>> computer) -> TableViewBuilder &;

    /**
     * @brief Adds multiple columns backed by a single multi-output computer.
     * @tparam T Element type of the outputs.
     * @param baseName Base name for the columns; per-output suffixes are provided by the computer.
     * @param computer Unique pointer to the multi-output computer.
     * @return Reference to this builder for method chaining.
     */
    template<typename T>
    auto addColumns(std::string const & baseName, std::unique_ptr<IMultiColumnComputer<T>> computer) -> TableViewBuilder &;

    /**
     * @brief Builds the final TableView object.
     * 
     * This method validates the configuration and constructs the TableView.
     * After calling build(), the builder is in an invalid state and should
     * not be used further.
     * 
     * @return The constructed TableView object.
     * @throws std::runtime_error if the configuration is invalid.
     */
    [[nodiscard]] auto build() -> TableView;

private:
    /**
     * @brief Validates that at most one multi-sample source is used.
     * 
     * This method checks all columns for dependencies on multi-sample line sources.
     * If more than one multi-sample source is found, it throws an exception with
     * detailed information about the conflicting sources.
     * 
     * @throws std::runtime_error if multiple multi-sample sources are detected.
     */
    void validateMultiSampleSources();

private:
    std::shared_ptr<DataManagerExtension> m_dataManager;
    std::unique_ptr<IRowSelector> m_rowSelector;
    std::vector<std::shared_ptr<IColumn>> m_columns;
};

// Template method implementation
template<typename T>
auto TableViewBuilder::addColumn(std::string const & name, std::unique_ptr<IColumnComputer<T>> computer) -> TableViewBuilder & {
    if (!computer) {
        throw std::invalid_argument("Column computer cannot be null");
    }

    // Create the templated column
    auto column = std::shared_ptr<IColumn>(new Column<T>(name, std::move(computer)));
    m_columns.push_back(std::move(column));

    return *this;
}

template<typename T>
auto TableViewBuilder::addColumns(std::string const & baseName, std::unique_ptr<IMultiColumnComputer<T>> computer) -> TableViewBuilder & {
    if (!computer) {
        throw std::invalid_argument("Multi-column computer cannot be null");
    }

    auto suffixes = computer->getOutputNames();
    if (suffixes.empty()) {
        throw std::invalid_argument("Multi-column computer returned no outputs");
    }

    // wrap in shared_ptr so each per-output view can reference the same instance
    auto sharedComputer = std::shared_ptr<IMultiColumnComputer<T>>(std::move(computer));
    auto sharedCache = std::make_shared<typename MultiComputerOutputView<T>::SharedBatchCache>();

    for (size_t i = 0; i < suffixes.size(); ++i) {
        std::string colName = baseName + suffixes[i];
        auto view = std::make_unique<MultiComputerOutputView<T>>(sharedComputer, sharedCache, i);
        auto column = std::shared_ptr<IColumn>(new Column<T>(colName, std::move(view)));
        m_columns.push_back(std::move(column));
    }

    return *this;
}

#endif// TABLE_VIEW_BUILDER_H
