#ifndef COLUMNTYPEINFO_HPP
#define COLUMNTYPEINFO_HPP

#include <typeindex>
#include <variant>
#include <vector>
#include <string>

/**
 * @brief Variant type containing all possible column data types that TableView can return
 * 
 * This variant is based on the explicit template instantiations in TableManagerSource
 * and represents the complete set of types that the system supports.
 * TableView stores columns as std::vector<T> where T is the row element type.
 */
using ColumnDataVariant = std::variant<
    std::vector<float>,                    // Column of float values (one per row)
    std::vector<double>,                   // Column of double values (one per row)
    std::vector<int>,                      // Column of int values (one per row)
    std::vector<bool>,                     // Column of bool values (one per row)
    std::vector<std::string>,              // Column of string values (one per row)
    std::vector<std::vector<float>>        // Column of float vectors (one vector per row)
    // Add more types as needed based on your explicit instantiations
>;

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
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<bool>>() {
    return ColumnTypeInfo(typeid(std::vector<bool>), typeid(bool), 
                         true, false, "std::vector<bool>", "bool");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<std::string>>() {
    return ColumnTypeInfo(typeid(std::vector<std::string>), typeid(std::string), 
                         true, false, "std::vector<std::string>", "std::string");
}

template<>
inline ColumnTypeInfo ColumnTypeInfo::createTypeInfo<std::vector<std::vector<float>>>() {
    return ColumnTypeInfo(typeid(std::vector<std::vector<float>>), typeid(float), 
                         true, true, "std::vector<std::vector<float>>", "float");
}

/**
 * @brief Visitor pattern for type-safe column data access
 * 
 * This allows consumers to handle different column types in a type-safe manner
 * without try/catch blocks. Use operator() overloads for std::visit compatibility.
 */
template<typename ReturnType = void>
class ColumnDataVisitor {
public:
    virtual ~ColumnDataVisitor() = default;
    
    virtual ReturnType operator()(const std::vector<float>& data) = 0;
    virtual ReturnType operator()(const std::vector<double>& data) = 0;
    virtual ReturnType operator()(const std::vector<int>& data) = 0;
    virtual ReturnType operator()(const std::vector<bool>& data) = 0;
    virtual ReturnType operator()(const std::vector<std::string>& data) = 0;
    virtual ReturnType operator()(const std::vector<std::vector<float>>& data) = 0;
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
