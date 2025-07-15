#ifndef TABLE_VIEW_BUILDER_H
#define TABLE_VIEW_BUILDER_H

#include "TableView.h"
#include "Column.h"
#include "IRowSelector.h"
#include "IColumnComputer.h"
#include "DataManagerExtension.h"

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
    auto setRowSelector(std::unique_ptr<IRowSelector> rowSelector) -> TableViewBuilder&;

    /**
     * @brief Adds a column to the table being built.
     * @param name The name of the column.
     * @param computer Unique pointer to the column computer.
     * @return Reference to this builder for method chaining.
     */
    auto addColumn(const std::string& name, std::unique_ptr<IColumnComputer> computer) -> TableViewBuilder&;

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
    std::vector<std::pair<std::string, std::unique_ptr<IColumnComputer>>> m_columns;
};

#endif // TABLE_VIEW_BUILDER_H
