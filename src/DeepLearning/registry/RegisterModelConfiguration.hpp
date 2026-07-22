/**
 * @file RegisterModelConfiguration.hpp
 * @brief RAII helper for static registration of per-model configuration hooks.
 */

#ifndef NEURALYZER_REGISTER_MODEL_CONFIGURATION_HPP
#define NEURALYZER_REGISTER_MODEL_CONFIGURATION_HPP

#include "ModelRegistry.hpp"
#include "models_v2/ModelBase.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl/DefaultIfMissing.hpp>
#include <rfl/json.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace dl {

/**
 * @brief RAII helper for compile-time model configuration registration.
 *
 * @tparam ModelClass Concrete @c ModelBase subclass.
 * @tparam Params reflect-cpp parameter struct owned by the model.
 */
template<typename ModelClass, typename Params>
class RegisterModelConfiguration {
public:
    /**
     * @brief Register configuration hooks for an already-registered model.
     *
     * @param model_id Registry key matching @c ModelClass::modelId().
     * @param apply_typed Applies typed params to a concrete model instance.
     * @param is_complete Returns whether persisted params are complete.
     */
    RegisterModelConfiguration(
            std::string model_id,
            std::function<void(ModelClass &, Params const &)> apply_typed,
            std::function<bool(Params const &)> is_complete,
            std::function<std::string(std::string const &)> form_json_from_stored =
                    nullptr,
            std::function<std::string(std::string const &, bool)>
                    stored_json_from_form = nullptr) {

        auto apply = [apply_typed = std::move(apply_typed),
                      model_id = model_id](
                             ModelBase & model,
                             std::string const & json) {
            auto parsed =
                    rfl::json::read<Params, rfl::DefaultIfMissing>(json);
            if (!parsed) {
                throw std::runtime_error(
                        "Model '" + model_id +
                        "': failed to parse configuration JSON");
            }
            auto * typed = dynamic_cast<ModelClass *>(&model);
            if (typed == nullptr) {
                throw std::runtime_error(
                        "Model '" + model_id +
                        "': configuration apply type mismatch");
            }
            apply_typed(*typed, *parsed);
        };

        auto is_complete_fn = [is_complete = std::move(is_complete)](
                                      std::string const & json) -> bool {
            auto parsed =
                    rfl::json::read<Params, rfl::DefaultIfMissing>(json);
            if (!parsed) {
                return false;
            }
            return is_complete(*parsed);
        };

        ModelConfigurationEntry entry;
        entry.schema = extractParameterSchema<Params>();
        entry.default_json = [] {
            return rfl::json::write(Params{});
        };
        entry.apply = std::move(apply);
        entry.is_complete = std::move(is_complete_fn);
        if (form_json_from_stored) {
            entry.form_json_from_stored = std::move(form_json_from_stored);
        }
        if (stored_json_from_form) {
            entry.stored_json_from_form = std::move(stored_json_from_form);
        }

        ModelRegistry::instance().registerConfiguration(
                model_id, std::move(entry));
    }
};

}// namespace dl

/**
 * @brief Register a model factory and optional configuration hooks.
 *
 * Place at namespace scope in the model's @c .cpp file.
 */
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
#define DL_REGISTER_MODEL_WITH_CONFIG(                                         \
        ModelClass, Params, apply_fn, is_complete_fn, form_from_stored_fn,       \
        stored_from_form_fn)                                                   \
    namespace {                                                                \
    [[maybe_unused]] bool const registered_##ModelClass = [] {               \
        auto instance = std::make_unique<ModelClass>();                        \
        auto id = instance->modelId();                                         \
        dl::ModelRegistry::instance().registerModel(                             \
                id, [] { return std::make_unique<ModelClass>(); });           \
        dl::RegisterModelConfiguration<ModelClass, Params>(                    \
                id, apply_fn, is_complete_fn, form_from_stored_fn,             \
                stored_from_form_fn);                                        \
        return true;                                                           \
    }();                                                                       \
    }
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#endif// NEURALYZER_REGISTER_MODEL_CONFIGURATION_HPP
