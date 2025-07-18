#ifndef TABLE_VIEW_BUILDER_H
#define TABLE_VIEW_BUILDER_H

#include "TableView.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/columns/Column.h"
#include "utils/TableView/columns/IColumn.h"
#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IRowSelector.h"

#include <memory>
#include <string>
#include <vector>

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

#endif// TABLE_VIEW_BUILDER_H
