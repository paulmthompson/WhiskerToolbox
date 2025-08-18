#ifndef ICOLUMN_H
#define ICOLUMN_H

#include <string>
#include <typeinfo>
#include <vector>

// Forward declaration
class TableView;

/**
 * @brief Non-templated base interface for all column types.
 * 
 * This interface provides type erasure for the TableView system, allowing
 * it to manage columns of different types polymorphically. The actual
 * typed operations are handled by the templated Column<T> class.
 */
class IColumn {
public:
    virtual ~IColumn() = default;

    // Make this class non-copyable and non-movable since it's a pure interface
    IColumn(IColumn const &) = delete;
    IColumn & operator=(IColumn const &) = delete;
    IColumn(IColumn &&) = delete;
    IColumn & operator=(IColumn &&) = delete;

    /**
     * @brief Gets the name of this column.
     * @return The column name.
     */
    [[nodiscard]] virtual auto getName() const -> std::string const & = 0;

    /**
     * @brief Gets the type information for this column.
     * @return The std::type_info for the column's data type.
     */
    [[nodiscard]] virtual auto getType() const -> std::type_info const & = 0;

    /**
     * @brief Triggers computation of the column data without exposing the type.
     * 
     * This method is used by the TableView to materialize columns during
     * dependency resolution without needing to know the specific type.
     * 
     * @param table Pointer to the TableView that owns this column.
     */
    virtual void materialize(TableView * table) = 0;

    /**
     * @brief Gets the source dependency for this column.
     * @return The name of the required data source.
     */
    [[nodiscard]] virtual auto getSourceDependency() const -> std::string = 0;

    /**
     * @brief Gets the column dependencies for this column.
     * @return Vector of column names this column depends on.
     */
    [[nodiscard]] virtual std::vector<std::string> getDependencies() const = 0;

    /**
     * @brief Checks if the column data has been materialized.
     * @return True if data is cached, false otherwise.
     */
    [[nodiscard]] virtual auto isMaterialized() const -> bool = 0;

    /**
     * @brief Clears the cached data, forcing recomputation on next access.
     */
    virtual void clearCache() = 0;

protected:
    // Protected constructor to prevent direct instantiation
    IColumn() = default;
};

#endif// ICOLUMN_H
