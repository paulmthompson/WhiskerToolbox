#ifndef WHISKERTOOLBOX_MLMODELREGISTRY_HPP
#define WHISKERTOOLBOX_MLMODELREGISTRY_HPP

#include "MLModelOperation.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

class MLModelRegistry {
public:
    MLModelRegistry();

    // Non-copyable and non-movable
    MLModelRegistry(MLModelRegistry const &) = delete;
    MLModelRegistry & operator=(MLModelRegistry const &) = delete;
    MLModelRegistry(MLModelRegistry &&) = delete;
    MLModelRegistry & operator=(MLModelRegistry &&) = delete;

    [[nodiscard]] std::vector<std::string> getAvailableModelNames() const;
    [[nodiscard]] MLModelOperation* findOperationByName(std::string const& operation_name) const;

private:
    void _registerModelOperation(std::unique_ptr<MLModelOperation> op);

    std::vector<std::unique_ptr<MLModelOperation>> _all_operations;
    std::map<std::string, MLModelOperation*> _name_to_operation;
};

#endif // WHISKERTOOLBOX_MLMODELREGISTRY_HPP 