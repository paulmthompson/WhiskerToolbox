#ifndef ICOLUMN_H
#define ICOLUMN_H

#include "Entity/EntityTypes.hpp"

#include <memory>
#include <string>
#include <typeinfo>
#include <variant>
#include <vector>

// Forward declaration
class TableView;

/**
 * @brief Enumeration of EntityID structures that columns can provide.
 */
enum class EntityIdStructure { 
    None,     ///< No EntityIDs available
    Simple,   ///< One EntityID per row (std::vector<EntityId>)
    Complex,  ///< Multiple EntityIDs per row (std::vector<std::vector<EntityId>>)
    Shared    ///< Shared references to EntityID collections (std::vector<SharedEntityIdCollection>)
};

/**
 * @brief Type aliases for EntityID collections and sharing.
 */
using EntityIdCollection = std::vector<EntityId>;
using SharedEntityIdCollection = std::shared_ptr<const EntityIdCollection>;

/**
 * @brief Variant type for different EntityID structures at the column level.
 */
using ColumnEntityIds = std::variant<
    std::monostate,                              ///< None: No EntityIDs
    std::vector<EntityId>,                       ///< Simple: One EntityID per row
    std::vector<std::vector<EntityId>>,          ///< Complex: Multiple EntityIDs per row
    std::vector<SharedEntityIdCollection>       ///< Shared: References to shared collections
>;

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

    /**
     * @brief Gets the EntityID structure type for this column.
     * 
     * This indicates whether the column provides no EntityIDs, simple EntityIDs
     * (one per row), complex EntityIDs (multiple per row), or shared EntityIDs
     * (references to shared collections).
     * 
     * @return The EntityID structure type for this column.
     */
    [[nodiscard]] virtual EntityIdStructure getEntityIdStructure() const = 0;

    /**
     * @brief Gets all EntityIDs for this column using the high-level variant approach.
     * 
     * The returned variant contains one of:
     * - std::monostate: No EntityIDs available
     * - std::vector<EntityId>: One EntityID per row (simple)
     * - std::vector<std::vector<EntityId>>: Multiple EntityIDs per row (complex)
     * - std::vector<SharedEntityIdCollection>: Shared EntityID collections
     * 
     * @param table Pointer to the TableView that owns this column.
     * @return ColumnEntityIds variant containing the EntityIDs for this column.
     */
    [[nodiscard]] virtual ColumnEntityIds getColumnEntityIds(TableView * table) const = 0;

    /**
     * @brief Convenience method to get EntityIDs for a specific row.
     * 
     * This method works across all EntityID structures and always returns
     * a vector of EntityIDs for the specified row. For shared structures,
     * it dereferences the shared pointer. For simple structures, it returns
     * a single-element vector.
     * 
     * @param table Pointer to the TableView that owns this column.
     * @param row_index The row index to get EntityIDs for.
     * @return Vector of EntityIDs for the specified row. Empty if not available.
     */
    [[nodiscard]] virtual std::vector<EntityId> getCellEntityIds(TableView * table, size_t row_index) const = 0;

    /**
     * @brief Checks if this column provides EntityID information.
     * 
     * This is a convenience method that checks if getEntityIdStructure() != None.
     * 
     * @return True if EntityIDs are available, false otherwise.
     */
    [[nodiscard]] virtual bool hasEntityIds() const {
        return getEntityIdStructure() != EntityIdStructure::None;
    }

protected:
    // Protected constructor to prevent direct instantiation
    IColumn() = default;
};

#endif// ICOLUMN_H
