#ifndef MLCORE_MLMODELREGISTRY_HPP
#define MLCORE_MLMODELREGISTRY_HPP

/**
 * @file MLModelRegistry.hpp
 * @brief Factory registry for ML model operations
 *
 * Maintains a collection of MLModelOperation instances and provides lookup
 * by name and filtering by task type. 
 *
 * ## Usage
 * ```cpp
 * MLModelRegistry registry;
 * // Built-in models are registered in the constructor
 *
 * // List classification models
 * auto names = registry.getModelNames(MLTaskType::BinaryClassification);
 *
 * // Create a fresh instance for training
 * auto model = registry.create("Random Forest");
 * auto params = model->getDefaultParameters();
 * model->train(features, labels, params.get());
 * ```
 *
 * ## Registration
 * Models are registered in the constructor. External code can also register
 * additional models via `registerModel()`.
 *
 */

#include "MLModelOperation.hpp"
#include "MLTaskType.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MLCore {

/**
 * @brief Factory registry for ML model operations
 *
 * Stores factory functions (not live instances) so that each `create()` call
 * returns a fresh, untrained model. This avoids issues with shared mutable
 * state between callers.
 */
class MLModelRegistry {
public:
    /**
     * @brief Factory function type that returns a new, untrained model instance
     */
    using FactoryFn = std::function<std::unique_ptr<MLModelOperation>()>;

    /**
     * @brief Construct the registry and register all built-in models
     *
     * Registers all six built-in models: RandomForest, NaiveBayes,
     * LogisticRegression, KMeans, DBSCAN, and GaussianMixture.
     */
    MLModelRegistry();

    // Non-copyable, non-movable (owns factory functions)
    MLModelRegistry(MLModelRegistry const &) = delete;
    MLModelRegistry & operator=(MLModelRegistry const &) = delete;
    MLModelRegistry(MLModelRegistry &&) = delete;
    MLModelRegistry & operator=(MLModelRegistry &&) = delete;

    // ========================================================================
    // Registration
    // ========================================================================

    /**
     * @brief Register a model factory
     *
     * @param name     Unique name for the model (e.g., "Random Forest").
     *                 If a model with this name already exists, it is replaced.
     * @param task     The ML task type this model supports
     * @param factory  Factory function that creates a fresh, untrained instance
     */
    void registerModel(
            std::string const & name,
            MLTaskType task,
            FactoryFn factory);

    /**
     * @brief Register by instantiating a model to extract metadata
     *
     * Creates one temporary instance to get the name and task type,
     * then stores the factory for future `create()` calls.
     *
     * @tparam ModelT  Concrete MLModelOperation subclass with a default constructor
     */
    template<typename ModelT>
    void registerModel() {
        auto prototype = std::make_unique<ModelT>();
        auto name = prototype->getName();
        auto task = prototype->getTaskType();
        registerModel(
                name,
                task,
                []() { return std::make_unique<ModelT>(); });
    }

    // ========================================================================
    // Query
    // ========================================================================

    /**
     * @brief Get names of all registered models
     */
    [[nodiscard]] std::vector<std::string> getAllModelNames() const;

    /**
     * @brief Get names of models that support a specific task type
     */
    [[nodiscard]] std::vector<std::string> getModelNames(MLTaskType task) const;

    /**
     * @brief Check if a model with the given name is registered
     */
    [[nodiscard]] bool hasModel(std::string const & name) const;

    /**
     * @brief Get the task type of a registered model
     *
     * @return The task type, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<MLTaskType> getTaskType(std::string const & name) const;

    /**
     * @brief Number of registered models
     */
    [[nodiscard]] std::size_t size() const;

    // ========================================================================
    // Factory
    // ========================================================================

    /**
     * @brief Create a new, untrained model instance by name
     *
     * @param name  The registered model name
     * @return A fresh model instance, or nullptr if the name is not found
     */
    [[nodiscard]] std::unique_ptr<MLModelOperation> create(std::string const & name) const;

private:
    /**
     * @brief Internal entry for a registered model
     */
    struct ModelEntry {
        std::string name;
        MLTaskType task;
        FactoryFn factory;
    };

    /// All registered models in insertion order
    std::vector<ModelEntry> _entries;

    /// Name → index into _entries for O(1) lookup
    std::unordered_map<std::string, std::size_t> _name_index;
};

}// namespace MLCore

#endif// MLCORE_MLMODELREGISTRY_HPP
