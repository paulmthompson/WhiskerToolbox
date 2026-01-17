#ifndef TRANSFORMREGISTRY_HPP
#define TRANSFORMREGISTRY_HPP

#include "data_transforms.hpp"
#include "datamanager_export.h"

#include <map>
#include <memory>// unique_ptr
#include <string>
#include <typeindex>
#include <vector>


class DATAMANAGER_EXPORT TransformRegistry {
public:
    TransformRegistry();

    // Make registry non-copyable and non-movable
    TransformRegistry(TransformRegistry const &) = delete;
    TransformRegistry & operator=(TransformRegistry const &) = delete;
    TransformRegistry(TransformRegistry &&) = delete;
    TransformRegistry & operator=(TransformRegistry &&) = delete;

    /**
     * @brief Gets the names of operations applicable to the data type
     * currently held within the provided variant.
     * @param dataVariant The actual data variant instance representing the selected data.
     * @return A vector of strings containing the names of applicable operations.
     * Returns empty vector if no operations target the held type,
     * or if the variant holds a non-operation-targetable type (like std::monostate).
     */
    std::vector<std::string> getOperationNamesForVariant(DataTypeVariant const & dataVariant) const;

    /**
     * @brief Finds an operation object by its registered name.
     * @param operation_name The exact name of the operation (e.g., "Calculate Area").
     * @return A non-owning pointer to the IOperation object, or nullptr if not found.
     */
    TransformOperation * findOperationByName(std::string const & operation_name) const;

private:
    // Stores the actual operation objects
    std::vector<std::unique_ptr<TransformOperation>> all_operations_;

    // Maps type_index(shared_ptr<T>) -> vector<Operation Name String>
    std::map<std::type_index, std::vector<std::string>> type_index_to_op_names_;

    // Maps operation name string -> IOperation* (for execution lookup)
    std::map<std::string, TransformOperation *> name_to_operation_;

    /**
     * @brief Registers a single operation instance.
     */
    void _registerOperation(std::unique_ptr<TransformOperation> op);

    /**
     * @brief Pre-computes the mapping from data type_index to applicable operation names
     * based *only* on the registered operations.
     */
    void _computeApplicableOperations();
};


#endif// TRANSFORMREGISTRY_HPP
