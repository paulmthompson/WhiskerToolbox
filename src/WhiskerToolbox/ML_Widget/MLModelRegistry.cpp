#include "MLModelRegistry.hpp"

#include "MLModelOperation.hpp"
#include "ML_Widget/ML_Naive_Bayes_Widget/NaiveBayesModelOperation.hpp"
#include "ML_Widget/ML_Random_Forest_Widget/RandomForestModelOperation.hpp"

#include <iostream>

MLModelRegistry::MLModelRegistry() {
    std::cout << "Initializing ML Model Registry..." << std::endl;
    _registerModelOperation(std::make_unique<NaiveBayesModelOperation>());
    _registerModelOperation(std::make_unique<RandomForestModelOperation>());
    std::cout << "ML Model Registry Initialized." << std::endl;
}

void MLModelRegistry::_registerModelOperation(std::unique_ptr<MLModelOperation> op) {
    if (!op) return;
    std::string op_name = op->getName();
    if (_name_to_operation.count(op_name)) {
        std::cerr << "Warning: ML Model Operation with name '" << op_name << "' already registered." << std::endl;
        return;
    }
    std::cout << "Registering ML Model operation: " << op_name << std::endl;
    _name_to_operation[op_name] = op.get();
    _all_operations.push_back(std::move(op));
}

std::vector<std::string> MLModelRegistry::getAvailableModelNames() const {
    std::vector<std::string> names;
    for (auto const& op : _all_operations) {
        if (op) {
            names.push_back(op->getName());
        }
    }
    // Sort or ensure order if necessary, for now, registration order is fine.
    return names;
}

MLModelOperation* MLModelRegistry::findOperationByName(std::string const& operation_name) const {
    auto it = _name_to_operation.find(operation_name);
    if (it != _name_to_operation.end()) {
        return it->second;
    }
    return nullptr;
} 
