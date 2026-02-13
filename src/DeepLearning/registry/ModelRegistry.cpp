#include "ModelRegistry.hpp"

#include "runtime/RuntimeModel.hpp"
#include "runtime/RuntimeModelSpec.hpp"

#include <algorithm>
#include <cassert>

namespace dl {

ModelRegistry & ModelRegistry::instance()
{
    static ModelRegistry registry;
    return registry;
}

void ModelRegistry::registerModel(std::string const & model_id, FactoryFn factory)
{
    std::lock_guard lock(_mutex);
    _factories[model_id] = std::move(factory);
    // Invalidate any cached info for this model since the factory may have changed.
    _info_cache.erase(model_id);
}

bool ModelRegistry::unregisterModel(std::string const & model_id)
{
    std::lock_guard lock(_mutex);
    auto const erased = _factories.erase(model_id);
    _info_cache.erase(model_id);
    return erased > 0;
}

std::vector<std::string> ModelRegistry::availableModels() const
{
    std::lock_guard lock(_mutex);
    std::vector<std::string> ids;
    ids.reserve(_factories.size());
    for (auto const & [id, _] : _factories) {
        ids.push_back(id);
    }
    // std::map is already sorted, but be explicit for the contract.
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::size_t ModelRegistry::size() const
{
    std::lock_guard lock(_mutex);
    return _factories.size();
}

bool ModelRegistry::hasModel(std::string const & model_id) const
{
    std::lock_guard lock(_mutex);
    return _factories.contains(model_id);
}

std::unique_ptr<ModelBase> ModelRegistry::create(std::string const & model_id) const
{
    std::lock_guard lock(_mutex);
    auto it = _factories.find(model_id);
    if (it == _factories.end()) {
        return nullptr;
    }
    return it->second();
}

std::optional<ModelRegistry::ModelInfo>
ModelRegistry::getModelInfo(std::string const & model_id) const
{
    std::lock_guard lock(_mutex);
    if (!_factories.contains(model_id)) {
        return std::nullopt;
    }
    ensureCached(model_id);
    return _info_cache.at(model_id);
}

TensorSlotDescriptor const *
ModelRegistry::getInputSlot(std::string const & model_id,
                            std::string const & slot_name) const
{
    std::lock_guard lock(_mutex);
    if (!_factories.contains(model_id)) {
        return nullptr;
    }
    ensureCached(model_id);
    auto const & info = _info_cache.at(model_id);
    for (auto const & slot : info.inputs) {
        if (slot.name == slot_name) {
            return &slot;
        }
    }
    return nullptr;
}

TensorSlotDescriptor const *
ModelRegistry::getOutputSlot(std::string const & model_id,
                             std::string const & slot_name) const
{
    std::lock_guard lock(_mutex);
    if (!_factories.contains(model_id)) {
        return nullptr;
    }
    ensureCached(model_id);
    auto const & info = _info_cache.at(model_id);
    for (auto const & slot : info.outputs) {
        if (slot.name == slot_name) {
            return &slot;
        }
    }
    return nullptr;
}

std::optional<std::string>
ModelRegistry::registerFromJson(std::filesystem::path const & json_path,
                                std::string * error_out)
{
    auto spec_result = RuntimeModelSpec::fromJsonFile(json_path);
    if (!spec_result) {
        if (error_out != nullptr) {
            *error_out = spec_result.error()->what();
        }
        return std::nullopt;
    }

    auto spec = std::move(spec_result.value());

    auto validation_errors = spec.validate();
    if (!validation_errors.empty()) {
        if (error_out != nullptr) {
            std::string combined = "Validation failed:";
            for (auto const & err : validation_errors) {
                combined += " " + err + ";";
            }
            *error_out = combined;
        }
        return std::nullopt;
    }

    auto model_id = spec.model_id;
    auto shared_spec = std::make_shared<RuntimeModelSpec>(std::move(spec));

    registerModel(
        model_id,
        [shared_spec] {
            return std::make_unique<RuntimeModel>(*shared_spec);
        });

    return model_id;
}

void ModelRegistry::clear()
{
    std::lock_guard lock(_mutex);
    _factories.clear();
    _info_cache.clear();
}

void ModelRegistry::ensureCached(std::string const & model_id) const
{
    // Caller must hold _mutex.
    if (_info_cache.contains(model_id)) {
        return;
    }

    auto it = _factories.find(model_id);
    assert(it != _factories.end());

    auto model = it->second();
    assert(model != nullptr);

    ModelInfo info;
    info.model_id = model->modelId();
    info.display_name = model->displayName();
    info.description = model->description();
    info.inputs = model->inputSlots();
    info.outputs = model->outputSlots();
    info.preferred_batch_size = model->preferredBatchSize();
    info.max_batch_size = model->maxBatchSize();

    _info_cache[model_id] = std::move(info);
}

} // namespace dl
