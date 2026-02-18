#include "MLModelRegistry.hpp"

#include "supervised/NaiveBayesOperation.hpp"
#include "supervised/RandomForestOperation.hpp"

#include <algorithm>

namespace MLCore {

// ============================================================================
// Construction
// ============================================================================

MLModelRegistry::MLModelRegistry()
{
    // Built-in model registrations — added as models are implemented (Phase 2)
    registerModel<RandomForestOperation>();
    registerModel<NaiveBayesOperation>();
    // Future:
    //   registerModel<LogisticRegressionOperation>();
    //   registerModel<KMeansOperation>();
    //   registerModel<DBSCANOperation>();
    //   registerModel<GaussianMixtureOperation>();
}

// ============================================================================
// Registration
// ============================================================================

void MLModelRegistry::registerModel(
    std::string const & name,
    MLTaskType task,
    FactoryFn factory)
{
    auto it = _name_index.find(name);
    if (it != _name_index.end()) {
        // Replace existing entry
        _entries[it->second] = ModelEntry{name, task, std::move(factory)};
    } else {
        // Insert new entry
        _name_index[name] = _entries.size();
        _entries.push_back(ModelEntry{name, task, std::move(factory)});
    }
}

// ============================================================================
// Query
// ============================================================================

std::vector<std::string> MLModelRegistry::getAllModelNames() const
{
    std::vector<std::string> names;
    names.reserve(_entries.size());
    for (auto const & entry : _entries) {
        names.push_back(entry.name);
    }
    return names;
}

std::vector<std::string> MLModelRegistry::getModelNames(MLTaskType task) const
{
    std::vector<std::string> names;
    for (auto const & entry : _entries) {
        if (entry.task == task) {
            names.push_back(entry.name);
        }
    }
    return names;
}

bool MLModelRegistry::hasModel(std::string const & name) const
{
    return _name_index.contains(name);
}

std::optional<MLTaskType> MLModelRegistry::getTaskType(std::string const & name) const
{
    auto it = _name_index.find(name);
    if (it == _name_index.end()) {
        return std::nullopt;
    }
    return _entries[it->second].task;
}

std::size_t MLModelRegistry::size() const
{
    return _entries.size();
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<MLModelOperation> MLModelRegistry::create(std::string const & name) const
{
    auto it = _name_index.find(name);
    if (it == _name_index.end()) {
        return nullptr;
    }
    return _entries[it->second].factory();
}

} // namespace MLCore
