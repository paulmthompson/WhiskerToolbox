#ifndef WHISKERTOOLBOX_MODEL_REGISTRY_HPP
#define WHISKERTOOLBOX_MODEL_REGISTRY_HPP

#include "models_v2/ModelBase.hpp"
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

/// A lightweight compile-time registry of available ModelBase subclasses,
/// queryable by model ID and enumerable by the UI.
///
/// Models self-register via `registerModel()` at static-init time or
/// explicitly during application startup. The UI queries `availableModels()`
/// to populate combo boxes and `getModelInfo()` to display slot metadata
/// without constructing the full model.
///
/// Thread-safe: all methods are protected by a mutex.
///
/// Usage:
/// @code
///     // Registration (typically in the model's .cpp file):
///     DL_REGISTER_MODEL(MyModel)
///
///     // Or manually:
///     dl::ModelRegistry::instance().registerModel(
///         "my_model",
///         [] { return std::make_unique<MyModel>(); });
///
///     // Querying:
///     auto ids = dl::ModelRegistry::instance().availableModels();
///     auto info = dl::ModelRegistry::instance().getModelInfo("my_model");
///     auto model = dl::ModelRegistry::instance().create("my_model");
/// @endcode
class ModelRegistry {
public:
    using FactoryFn = std::function<std::unique_ptr<ModelBase>()>;

    /// Aggregated metadata for a registered model.
    /// Lazily populated on first query by constructing a temporary instance.
    struct ModelInfo {
        std::string model_id;
        std::string display_name;
        std::string description;
        std::vector<TensorSlotDescriptor> inputs;
        std::vector<TensorSlotDescriptor> outputs;
        int preferred_batch_size = 0;
        int max_batch_size = 0;
    };

    /// Get the singleton instance.
    static ModelRegistry & instance();

    /// Register a model factory by ID.
    ///
    /// If a model with the same ID is already registered, it is silently
    /// overwritten (useful for hot-reload / runtime JSON override).
    void registerModel(std::string const & model_id, FactoryFn factory);

    /// Remove a previously registered model by ID.
    /// Returns true if the model was found and removed.
    bool unregisterModel(std::string const & model_id);

    /// Get a sorted list of all registered model IDs.
    [[nodiscard]] std::vector<std::string> availableModels() const;

    /// Get the number of registered models.
    [[nodiscard]] std::size_t size() const;

    /// Check if a model ID is registered.
    [[nodiscard]] bool hasModel(std::string const & model_id) const;

    /// Instantiate a model by ID.
    /// Returns nullptr if the ID is not registered.
    [[nodiscard]] std::unique_ptr<ModelBase> create(std::string const & model_id) const;

    /// Query aggregated metadata for a model without keeping the instance.
    /// Returns std::nullopt if the model ID is not registered.
    [[nodiscard]] std::optional<ModelInfo> getModelInfo(std::string const & model_id) const;

    /// Look up a specific input slot descriptor for a model.
    /// Returns nullptr if the model or slot is not found.
    [[nodiscard]] TensorSlotDescriptor const *
    getInputSlot(std::string const & model_id, std::string const & slot_name) const;

    /// Look up a specific output slot descriptor for a model.
    /// Returns nullptr if the model or slot is not found.
    [[nodiscard]] TensorSlotDescriptor const *
    getOutputSlot(std::string const & model_id, std::string const & slot_name) const;

    /// Load a JSON model spec from a file and register the resulting RuntimeModel.
    ///
    /// On success, returns the model_id. On failure returns std::nullopt.
    /// If @p error_out is non-null, it receives the error message on failure.
    [[nodiscard]] std::optional<std::string>
    registerFromJson(std::filesystem::path const & json_path,
                     std::string * error_out = nullptr);

    /// Remove all registered models and clear the info cache.
    /// Primarily useful for testing.
    void clear();

private:
    ModelRegistry() = default;

    /// Ensure the ModelInfo cache is populated for the given model_id.
    /// Must be called with _mutex held.
    void ensureCached(std::string const & model_id) const;

    mutable std::mutex _mutex;
    std::map<std::string, FactoryFn> _factories;

    /// Cache: model_id → ModelInfo (lazily populated on first query).
    mutable std::map<std::string, ModelInfo> _info_cache;
};

} // namespace dl

/// Helper macro for convenient self-registration of a ModelBase subclass.
///
/// Place this in the model's .cpp file (at namespace scope). It creates a
/// static bool whose initializer registers the model at program startup.
///
/// Usage:
/// @code
///     // In NeuroSAMModel.cpp:
///     DL_REGISTER_MODEL(dl::NeuroSAMModel)
/// @endcode
///
/// The model class must have a default constructor and implement `modelId()`.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
#define DL_REGISTER_MODEL(ModelClass)                                          \
    namespace {                                                                \
    [[maybe_unused]] bool const registered_##ModelClass = [] {                 \
        auto instance = std::make_unique<ModelClass>();                         \
        auto id = instance->modelId();                                         \
        dl::ModelRegistry::instance().registerModel(                           \
            std::move(id),                                                     \
            [] { return std::make_unique<ModelClass>(); });                     \
        return true;                                                           \
    }();                                                                       \
    }
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#endif // WHISKERTOOLBOX_MODEL_REGISTRY_HPP
