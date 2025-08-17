#ifndef COLUMNTYPEINFO_HPP
#define COLUMNTYPEINFO_HPP

#include "TimeFrame/TimeFrame.hpp"
#include "utils/metaprogramming_utils.hpp"

#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <variant>
#include <vector>


// ==========================================
// Supported tableview column data types
// ==========================================

// Element types that can be stored in columns (what each row contains)
using SupportedColumnElementTypes = std::tuple<
    float,
    double,
    int,
    int64_t,
    bool,
    std::vector<float>,
    std::vector<double>,
    std::vector<int>,
    std::vector<TimeFrameIndex>
>;

// Data types that getColumnDataVariant() can return (always vectors)
using SupportedColumnDataTypes = std::tuple<
    std::vector<float>,
    std::vector<double>,
    std::vector<int>,
    std::vector<int64_t>,
    std::vector<bool>,
    std::vector<TimeFrameIndex>,
    std::vector<std::vector<float>>,
    std::vector<std::vector<double>>,
    std::vector<std::vector<int>>,
    std::vector<std::vector<TimeFrameIndex>>
>;

// Backward compatibility alias
using SupportedColumnTypes = SupportedColumnElementTypes;

template <typename T>
concept SupportedColumnType = is_in_tuple_v<T, SupportedColumnTypes>;

/**
 * @brief Variant type containing all possible column data types that TableView can return
 * 
 * This variant represents the complete set of types that getColumnDataVariant() can return.
 * These are always vectors since getColumnValues<T>() returns std::vector<T>.
 */
using ColumnDataVariant = tuple_to_variant_t<SupportedColumnDataTypes>;
using ColumnElementVariant = tuple_to_variant_t<SupportedColumnElementTypes>;

/**
 * @brief Runtime type information for table columns
 * 
 * This stores the essential type information needed for consumers to understand
 * column data without making assumptions about how they'll use it.
 */
struct ColumnTypeInfo {
    std::type_index actualType;           ///< The actual std::type_index of the column data
    std::type_index elementType;          ///< For vectors, the element type; same as actualType for scalars
    bool isVectorType;                    ///< True if the type is std::vector<T>
    bool isNestedVector;                  ///< True if the type is std::vector<std::vector<T>>
    std::string typeName;                 ///< Human-readable type name
    std::string elementTypeName;          ///< Human-readable element type name
    
    // Default constructor
    ColumnTypeInfo() 
        : actualType(typeid(void)), elementType(typeid(void)), 
          isVectorType(false), isNestedVector(false), 
          typeName("void"), elementTypeName("void") {}
    
    // Constructor from type indices
    ColumnTypeInfo(std::type_index actual_type, std::type_index element_type, 
                   bool is_vector, bool is_nested, 
                   std::string type_name, std::string element_name)
        : actualType(actual_type), elementType(element_type),
          isVectorType(is_vector), isNestedVector(is_nested),
          typeName(std::move(type_name)), elementTypeName(std::move(element_name)) {}
    
    /**
     * @brief Create ColumnTypeInfo from a template type
     * @tparam T The column data type
     * @return ColumnTypeInfo describing the type
     */
    template<typename T>
    static ColumnTypeInfo fromType() {
        return createTypeInfo<T>();
    }
    
    /**
     * @brief Check if this type matches a specific type
     * @tparam T The type to check against
     * @return True if the column is of type T
     */
    template<typename T>
    bool isType() const {
        return actualType == typeid(T);
    }
    
    /**
     * @brief Check if the element type matches a specific type
     * @tparam T The element type to check against
     * @return True if the column elements are of type T
     */
    template<typename T>
    bool hasElementType() const {
        return elementType == typeid(T);
    }

private:
    template<typename T>
    static ColumnTypeInfo createTypeInfo();
};

// Template specializations for createTypeInfo

// Scalar types (Column<T> where each row contains one T value)
template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<float>() {
    return ColumnTypeInfo(typeid(float), typeid(float), 
                         false, false, "float", "float");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<double>() {
    return ColumnTypeInfo(typeid(double), typeid(double), 
                         false, false, "double", "double");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<int>() {
    return ColumnTypeInfo(typeid(int), typeid(int), 
                         false, false, "int", "int");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<int64_t>() {
    return ColumnTypeInfo(typeid(int64_t), typeid(int64_t), 
                         false, false, "int64_t", "int64_t");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<bool>() {
    return ColumnTypeInfo(typeid(bool), typeid(bool), 
                         false, false, "bool", "bool");
}

// Vector types (Column<std::vector<T>> where each row contains a vector of T values)
template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<float>>() {
    return ColumnTypeInfo(typeid(std::vector<float>), typeid(float), 
                         true, false, "std::vector<float>", "float");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<double>>() {
    return ColumnTypeInfo(typeid(std::vector<double>), typeid(double), 
                         true, false, "std::vector<double>", "double");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<int>>() {
    return ColumnTypeInfo(typeid(std::vector<int>), typeid(int), 
                         true, false, "std::vector<int>", "int");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<TimeFrameIndex>>() {
    return ColumnTypeInfo(typeid(std::vector<TimeFrameIndex>), typeid(TimeFrameIndex), 
                         true, false, "std::vector<TimeFrameIndex>", "TimeFrameIndex");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<bool>>() {
    return ColumnTypeInfo(typeid(std::vector<bool>), typeid(bool), 
                         true, false, "std::vector<bool>", "bool");
}

/**
 * @brief Visitor pattern for type-safe column data access
 * 
 * This allows consumers to handle different column types in a type-safe manner
 * without try/catch blocks.
 */
template<typename ReturnType = void>
class ColumnDataVisitor {
public:
    virtual ~ColumnDataVisitor() = default;
    
    // Scalar types
    virtual ReturnType visit(const float& data) = 0;
    virtual ReturnType visit(const double& data) = 0;
    virtual ReturnType visit(const int& data) = 0;
    virtual ReturnType visit(const int64_t& data) = 0;
    virtual ReturnType visit(const bool& data) = 0;
    
    // Vector types
    virtual ReturnType visit(const std::vector<float>& data) = 0;
    virtual ReturnType visit(const std::vector<double>& data) = 0;
    virtual ReturnType visit(const std::vector<int>& data) = 0;
    virtual ReturnType visit(const std::vector<TimeFrameIndex>& data) = 0;
};

/**
 * @brief Apply a visitor to a ColumnDataVariant
 * @tparam Visitor The visitor type
 * @param variant The column data variant
 * @param visitor The visitor instance
 * @return The result of the visitor
 */
template<typename Visitor>
auto visitColumnData(const ColumnDataVariant& variant, Visitor&& visitor) {
    return std::visit(std::forward<Visitor>(visitor), variant);
}

#endif // COLUMNTYPEINFO_HPP
