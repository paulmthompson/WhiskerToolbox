#ifndef COLUMN_H
#define COLUMN_H

#include "ColumnTypeInfo.hpp"
#include "IColumn.h"
#include "Entity/EntityTypes.hpp"

#include <memory>
#include <string>
#include <typeinfo>
#include <variant>
#include <vector>

template<typename>
class IColumnComputer;

class TableView;

/**
 * @brief Templated column class that supports heterogeneous data types.
 * 
 * This class inherits from IColumn to provide type erasure while maintaining
 * type safety for the actual data storage and computation. It supports any
 * type T that can be stored in a std::vector<T>.
 */
template<SupportedColumnType T>
class Column : public IColumn {
public:
    /**
     * @brief Gets the values of this column.
     * 
     * This method returns a reference to the column's data vector, triggering
     * computation if the data is not yet materialized.
     * 
     * Each index in this vector corresponds to a row in the TableView,
     * and the values are computed based on the current row selection.
     * 
     * @param table Pointer to the TableView that owns this column.
     * @return Reference to the column's data vector.
     */
    [[nodiscard]] auto getValues(TableView * table) -> std::vector<T> const &;

    /**
     * @brief Triggers computation of the column data without exposing the type.
     * 
     * This method is used by the TableView to materialize columns during
     * dependency resolution without needing to know the specific type.
     * 
     * @param table Pointer to the TableView that owns this column.
     */
    void materialize(TableView * table) override;

    /**
     * @brief Gets the name of this column.
     * @return The column name.
     */
    [[nodiscard]] auto getName() const -> std::string const & override {
        return m_name;
    }

    /**
     * @brief Gets the type information for this column.
     * @return The std::type_info for the column's data type.
     */
    [[nodiscard]] auto getType() const -> std::type_info const & override {
        return typeid(T);
    }

    /**
     * @brief Gets the source dependency for this column.
     * @return The name of the required data source.
     */
    [[nodiscard]]  std::string getSourceDependency() const override;

    /**
     * @brief Gets the column dependencies for this column.
     * @return Vector of column names this column depends on.
     */
    [[nodiscard]] std::vector<std::string> getDependencies() const override;

    /**
     * @brief Checks if the column data has been materialized.
     * @return True if data is cached, false otherwise.
     */
    [[nodiscard]] bool isMaterialized() const override;

    /**
     * @brief Clears the cached data, forcing recomputation on next access.
     */
    void clearCache() override {
        m_cache = std::monostate{};
    }

    /**
     * @brief Checks if this column can provide EntityID information.
     * @return True if EntityIDs are available, false otherwise.
     */
    [[nodiscard]] bool hasEntityIds() const override;

    /**
     * @brief Gets EntityIDs for each row in this column.
     * @param table Pointer to the TableView that owns this column.
     * @return Vector of EntityIDs, one per row. Empty if not available.
     */
    [[nodiscard]] std::vector<EntityId> getEntityIds(TableView * table) const override;

    /**
     * @brief Gets all contributing EntityIDs for a specific row in this column.
     * @param table Pointer to the TableView that owns this column.
     * @param row_index The row index to get EntityIDs for.
     * @return Vector of EntityIDs that contributed to this cell. Empty if not available.
     */
    [[nodiscard]] std::vector<EntityId> getRowEntityIds(TableView * table, size_t row_index) const override;

private:
    friend class TableViewBuilder;

    /**
     * @brief Private constructor for TableViewBuilder.
     * @param name The name of the column.
     * @param computer The computation strategy for this column.
     */
    Column(std::string name, std::unique_ptr<IColumnComputer<T>> computer);

    std::string m_name;
    std::unique_ptr<IColumnComputer<T>> m_computer;
    std::variant<std::monostate, std::vector<T>> m_cache;
};


#endif// COLUMN_TEMPLATED_H
