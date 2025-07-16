#ifndef COLUMN_H
#define COLUMN_H

#include "IColumn.h"
#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IAnalogSource.h"

#include <memory>
#include <string>
#include <typeinfo>
#include <variant>
#include <vector>

// Forward declaration
class TableView;

/**
 * @brief Templated column class that supports heterogeneous data types.
 * 
 * This class inherits from IColumn to provide type erasure while maintaining
 * type safety for the actual data storage and computation. It supports any
 * type T that can be stored in a std::vector<T>.
 */
template<typename T>
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
    [[nodiscard]] auto getValues(TableView* table) -> const std::vector<T>&;

    void materialize(TableView* table) override;

    // IColumn interface implementation
    [[nodiscard]] auto getName() const -> const std::string& override { 
        return m_name; 
    }

    [[nodiscard]] auto getType() const -> const std::type_info& override { 
        return typeid(T); 
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_computer->getSourceDependency();
    }

    [[nodiscard]] auto getDependencies() const -> std::vector<std::string> override {
        return m_computer->getDependencies();
    }

    [[nodiscard]] auto isMaterialized() const -> bool override {
        return std::holds_alternative<std::vector<T>>(m_cache);
    }

    void clearCache() override {
        m_cache = std::monostate{};
    }

private:
    friend class TableViewBuilder;

    /**
     * @brief Private constructor for TableViewBuilder.
     * @param name The name of the column.
     * @param computer The computation strategy for this column.
     */
    Column(std::string name, std::unique_ptr<IColumnComputer<T>> computer)
        : m_name(std::move(name)), m_computer(std::move(computer)) {}

    std::string m_name;
    std::unique_ptr<IColumnComputer<T>> m_computer;
    std::variant<std::monostate, std::vector<T>> m_cache;
};


#endif // COLUMN_TEMPLATED_H
