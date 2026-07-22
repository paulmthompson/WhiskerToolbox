/**
 * @file ModelRegistry.hpp
 * @brief Compile-time registry of available ModelBase subclasses.
 */

#ifndef NEURALYZER_MODEL_REGISTRY_HPP
#define NEURALYZER_MODEL_REGISTRY_HPP

#include "ParameterSchema/ParameterSchema.hpp"
#include "models_v2/ModelBase.hpp"
#include "models_v2/ModelInfo.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief Optional per-model configuration hooks registered at static init.
 */
struct ModelConfigurationEntry {
    ParameterSchema schema;
    std::function<std::string()> default_json;
    std::function<void(ModelBase &, std::string const & json)> apply;
    std::function<bool(std::string const & json)> is_complete;
    /// Optional UI bridge: stored JSON -> form JSON for AutoParamWidget.
    std::function<std::string(std::string const & stored_json)> form_json_from_stored;
    /// Optional UI bridge: form JSON -> stored JSON for persistence.
    std::function<std::string(std::string const & form_json, bool applied)>
            stored_json_from_form;
};

/**
 * @brief A lightweight compile-time registry of available ModelBase subclasses,
 *        queryable by model ID and enumerable by the UI.
 *
 * Models self-register via `registerModel()` at static-init time or
 * explicitly during application startup. The UI queries `availableModels()`
 * to populate combo boxes and `getModelInfo()` to display slot metadata
 * without constructing the full model.
 *
 * Thread-safe: all methods are protected by a mutex.
 *
 * Usage:
 * @code
 *     // Registration (typically in the model's .cpp file):
 *     DL_REGISTER_MODEL(MyModel)
 *
 *     // Or manually:
 *     dl::ModelRegistry::instance().registerModel(
 *         "my_model",
 *         [] { return std::make_unique<MyModel>(); });
 *
 *     // Querying:
 *     auto ids = dl::ModelRegistry::instance().availableModels();
 *     auto info = dl::ModelRegistry::instance().getModelInfo("my_model");
 *     auto model = dl::ModelRegistry::instance().create("my_model");
 * @endcode
 */
class ModelRegistry {
public:
    using FactoryFn = std::function<std::unique_ptr<ModelBase>()>;

    /**
     * @brief Get the singleton instance.
     */
    static ModelRegistry & instance();

    /**
     * @brief Register a model factory by ID.
     *
     * If a model with the same ID is already registered, it is silently
     * overwritten (useful for hot-reload / runtime JSON override).
     */
    void registerModel(std::string const & model_id, FactoryFn factory);

    /**
     * @brief Register optional configuration hooks for a model.
     *
     * @pre A factory for @p model_id must already be registered.
     */
    void registerConfiguration(
            std::string const & model_id,
            ModelConfigurationEntry entry);

    /**
     * @brief Whether @p model_id has registered configuration hooks.
     */
    [[nodiscard]] bool hasConfiguration(std::string const & model_id) const;

    /**
     * @brief Query the parameter schema for a model's configuration.
     */
    [[nodiscard]] std::optional<ParameterSchema>
    getConfigurationSchema(std::string const & model_id) const;

    /**
     * @brief Default configuration JSON for a model.
     *
     * @return @c "{}" when the model has no registered configuration.
     */
    [[nodiscard]] std::string
    defaultConfigurationJson(std::string const & model_id) const;

    /**
     * @brief Apply persisted configuration JSON to a live model instance.
     */
    void applyConfiguration(
            std::string const & model_id,
            ModelBase & model,
            std::string const & json) const;

    /**
     * @brief Whether persisted configuration JSON is complete for weight loading.
     *
     * @return @c true when the model has no configuration hooks.
     */
    [[nodiscard]] bool configurationComplete(
            std::string const & model_id,
            std::string const & json) const;

    /**
     * @brief Convert stored configuration JSON to form JSON for AutoParamWidget.
     */
    [[nodiscard]] std::string configurationFormJson(
            std::string const & model_id,
            std::string const & stored_json) const;

    /**
     * @brief Convert form JSON to stored configuration JSON.
     */
    [[nodiscard]] std::string configurationStoredJson(
            std::string const & model_id,
            std::string const & form_json,
            bool applied) const;

    /**
     * @brief Remove a previously registered model by ID.
     *
     * @return True if the model was found and removed.
     */
    bool unregisterModel(std::string const & model_id);

    /**
     * @brief Get a sorted list of all registered model IDs.
     */
    [[nodiscard]] std::vector<std::string> availableModels() const;

    /**
     * @brief Get the number of registered models.
     */
    [[nodiscard]] std::size_t size() const;

    /**
     * @brief Check if a model ID is registered.
     */
    [[nodiscard]] bool hasModel(std::string const & model_id) const;

    /**
     * @brief Instantiate a model by ID.
     *
     * @return nullptr if the ID is not registered.
     */
    [[nodiscard]] std::unique_ptr<ModelBase> create(std::string const & model_id) const;

    /**
     * @brief Query aggregated metadata for a model without keeping the instance.
     *
     * @return std::nullopt if the model ID is not registered.
     */
    [[nodiscard]] std::optional<ModelInfo> getModelInfo(std::string const & model_id) const;

    /**
     * @brief Look up a specific input slot descriptor for a model.
     *
     * @return nullptr if the model or slot is not found.
     */
    [[nodiscard]] TensorSlotDescriptor const *
    getInputSlot(std::string const & model_id, std::string const & slot_name) const;

    /**
     * @brief Look up a specific output slot descriptor for a model.
     *
     * @return nullptr if the model or slot is not found.
     */
    [[nodiscard]] TensorSlotDescriptor const *
    getOutputSlot(std::string const & model_id, std::string const & slot_name) const;

    /**
     * @brief Load a JSON model spec from a file and register the resulting RuntimeModel.
     *
     * On success, returns the model_id. On failure returns std::nullopt.
     * If @p error_out is non-null, it receives the error message on failure.
     */
    [[nodiscard]] std::optional<std::string>
    registerFromJson(std::filesystem::path const & json_path,
                     std::string * error_out = nullptr);

    /**
     * @brief Remove all registered models and clear the info cache.
     *
     * Primarily useful for testing.
     */
    void clear();

private:
    ModelRegistry() = default;

    /**
     * @brief Ensure the ModelInfo cache is populated for the given model_id.
     *
     * @pre Caller must hold _mutex.
     */
    void ensureCached(std::string const & model_id) const;

    mutable std::mutex _mutex;
    std::map<std::string, FactoryFn> _factories;
    std::map<std::string, ModelConfigurationEntry> _configuration_entries;

    /** Cache: model_id → ModelInfo (lazily populated on first query). */
    mutable std::map<std::string, ModelInfo> _info_cache;
};

}// namespace dl

/**
 * @brief Helper macro for convenient self-registration of a ModelBase subclass.
 *
 * Place this in the model's .cpp file (at namespace scope). It creates a
 * static bool whose initializer registers the model at program startup.
 *
 * Usage:
 * @code
 *     // In NeuroSAMModel.cpp:
 *     DL_REGISTER_MODEL(dl::NeuroSAMModel)
 * @endcode
 *
 * The model class must have a default constructor and implement `modelId()`.
 */
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
#define DL_REGISTER_MODEL(ModelClass)                           \
    namespace {                                                 \
    [[maybe_unused]] bool const registered_##ModelClass = [] {  \
        auto instance = std::make_unique<ModelClass>();         \
        auto id = instance->modelId();                          \
        dl::ModelRegistry::instance().registerModel(            \
                std::move(id),                                  \
                [] { return std::make_unique<ModelClass>(); }); \
        return true;                                            \
    }();                                                        \
    }
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#endif// NEURALYZER_MODEL_REGISTRY_HPP
