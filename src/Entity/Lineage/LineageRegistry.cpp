#include "Entity/Lineage/LineageRegistry.hpp"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace WhiskerToolbox::Entity::Lineage {

void LineageRegistry::setLineage(std::string const & data_key, Descriptor lineage) {
    _lineages[data_key] = LineageEntry(std::move(lineage));
}

void LineageRegistry::removeLineage(std::string const & data_key) {
    _lineages.erase(data_key);
}

void LineageRegistry::clear() {
    _lineages.clear();
}

std::optional<Descriptor> LineageRegistry::getLineage(std::string const & data_key) const {
    auto it = _lineages.find(data_key);
    if (it != _lineages.end()) {
        return it->second.descriptor;
    }
    return std::nullopt;
}

std::optional<LineageEntry> LineageRegistry::getLineageEntry(std::string const & data_key) const {
    auto it = _lineages.find(data_key);
    if (it != _lineages.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool LineageRegistry::hasLineage(std::string const & data_key) const {
    return _lineages.find(data_key) != _lineages.end();
}

bool LineageRegistry::isSource(std::string const & data_key) const {
    auto it = _lineages.find(data_key);
    if (it == _lineages.end()) {
        // No lineage registered = treat as source
        return true;
    }
    return Lineage::isSource(it->second.descriptor);
}

std::vector<std::string> LineageRegistry::getSourceKeys(std::string const & data_key) const {
    auto it = _lineages.find(data_key);
    if (it == _lineages.end()) {
        return {};
    }
    return Lineage::getSourceKeys(it->second.descriptor);
}

std::vector<std::string> LineageRegistry::getDependentKeys(std::string const & source_key) const {
    std::vector<std::string> dependents;

    for (auto const & [key, entry]: _lineages) {
        auto sources = Lineage::getSourceKeys(entry.descriptor);
        if (std::find(sources.begin(), sources.end(), source_key) != sources.end()) {
            dependents.push_back(key);
        }
    }

    return dependents;
}

std::vector<std::string> LineageRegistry::getLineageChain(std::string const & data_key) const {
    std::vector<std::string> chain;
    std::unordered_set<std::string> visited;
    std::queue<std::string> to_visit;

    to_visit.push(data_key);

    while (!to_visit.empty()) {
        std::string current = to_visit.front();
        to_visit.pop();

        if (visited.count(current) > 0) {
            continue;// Already visited (handles cycles)
        }
        visited.insert(current);
        chain.push_back(current);

        // Get sources for current key
        auto sources = getSourceKeys(current);
        for (auto const & source: sources) {
            if (visited.count(source) == 0) {
                to_visit.push(source);
            }
        }
    }

    return chain;
}

std::vector<std::string> LineageRegistry::getAllKeys() const {
    std::vector<std::string> keys;
    keys.reserve(_lineages.size());

    for (auto const & [key, entry]: _lineages) {
        keys.push_back(key);
    }

    return keys;
}

void LineageRegistry::markStale(std::string const & data_key) {
    auto it = _lineages.find(data_key);
    if (it != _lineages.end()) {
        it->second.is_stale = true;
    }
}

void LineageRegistry::markValid(std::string const & data_key) {
    auto it = _lineages.find(data_key);
    if (it != _lineages.end()) {
        it->second.is_stale = false;
        it->second.last_validated = std::chrono::steady_clock::now();
    }
}

bool LineageRegistry::isStale(std::string const & data_key) const {
    auto it = _lineages.find(data_key);
    if (it == _lineages.end()) {
        // No lineage = treat as stale (unknown state)
        return true;
    }
    return it->second.is_stale;
}

void LineageRegistry::propagateStale(std::string const & data_key) {
    // Mark this key as stale
    markStale(data_key);

    // Build dependency map if needed and propagate
    auto dependents = getDependentKeys(data_key);
    for (auto const & dependent: dependents) {
        // Recursive propagation
        propagateStale(dependent);
    }

    // Invoke callback if registered
    if (_invalidation_callback) {
        auto sources = getSourceKeys(data_key);
        for (auto const & source: sources) {
            _invalidation_callback(data_key, source, SourceChangeType::DataModified);
        }
    }
}

void LineageRegistry::setInvalidationCallback(InvalidationCallback callback) {
    _invalidation_callback = std::move(callback);
}

std::unordered_map<std::string, std::vector<std::string>>
LineageRegistry::buildDependencyMap() const {
    std::unordered_map<std::string, std::vector<std::string>> dep_map;

    for (auto const & [key, entry]: _lineages) {
        auto sources = Lineage::getSourceKeys(entry.descriptor);
        for (auto const & source: sources) {
            dep_map[source].push_back(key);
        }
    }

    return dep_map;
}

}// namespace WhiskerToolbox::Entity::Lineage
