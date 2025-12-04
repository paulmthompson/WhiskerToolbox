#ifndef WHISKERTOOLBOX_LINEAGE_REGISTRY_HPP
#define WHISKERTOOLBOX_LINEAGE_REGISTRY_HPP

#include "Lineage/LineageTypes.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace WhiskerToolbox::Lineage {

/**
 * @brief Entry in the lineage registry with metadata
 */
struct LineageEntry {
    Descriptor descriptor;
    
    /// Whether this lineage may be out of sync with source data
    bool is_stale = false;
    
    /// When the lineage was last validated/created
    std::chrono::steady_clock::time_point last_validated;
    
    LineageEntry() = default;
    
    explicit LineageEntry(Descriptor desc)
        : descriptor(std::move(desc))
        , is_stale(false)
        , last_validated(std::chrono::steady_clock::now())
    {}
};

/**
 * @brief Type of change that occurred in source data
 */
enum class SourceChangeType {
    DataAdded,       ///< New elements added to source
    DataRemoved,     ///< Elements removed from source
    DataModified,    ///< Existing elements modified in place
    EntityIdsChanged ///< EntityIds were reassigned (e.g., rebuildAllEntityIds)
};

/**
 * @brief Callback for lineage invalidation events
 * 
 * @param derived_key The derived container whose lineage is affected
 * @param source_key The source container that changed
 * @param change_type What kind of change occurred
 */
using InvalidationCallback = std::function<void(
    std::string const& derived_key,
    std::string const& source_key,
    SourceChangeType change_type
)>;

/**
 * @brief Registry for container lineage metadata
 * 
 * Stores lineage descriptors that track parent-child relationships
 * between data containers. Supports staleness tracking and provides
 * query methods for lineage chain traversal.
 * 
 * Thread-safety: Not thread-safe. Caller must synchronize access.
 */
class LineageRegistry {
public:
    LineageRegistry() = default;
    ~LineageRegistry() = default;
    
    // Non-copyable, movable
    LineageRegistry(LineageRegistry const&) = delete;
    LineageRegistry& operator=(LineageRegistry const&) = delete;
    LineageRegistry(LineageRegistry&&) = default;
    LineageRegistry& operator=(LineageRegistry&&) = default;
    
    // ========== Registration ==========
    
    /**
     * @brief Register lineage for a data container
     * 
     * @param data_key Key of the derived container
     * @param lineage The lineage descriptor
     */
    void setLineage(std::string const& data_key, Descriptor lineage);
    
    /**
     * @brief Remove lineage for a data container
     * 
     * @param data_key Key of the container
     */
    void removeLineage(std::string const& data_key);
    
    /**
     * @brief Clear all lineage entries
     */
    void clear();
    
    // ========== Query ==========
    
    /**
     * @brief Get lineage descriptor for a container
     * 
     * @param data_key Key of the container
     * @return Lineage descriptor if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Descriptor> getLineage(std::string const& data_key) const;
    
    /**
     * @brief Get full lineage entry (including metadata) for a container
     * 
     * @param data_key Key of the container
     * @return Lineage entry if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<LineageEntry> getLineageEntry(std::string const& data_key) const;
    
    /**
     * @brief Check if a container has registered lineage
     */
    [[nodiscard]] bool hasLineage(std::string const& data_key) const;
    
    /**
     * @brief Check if a container is a source (no parent lineage)
     * 
     * Returns true if:
     * - No lineage is registered for this key
     * - Lineage is registered but is Source type
     */
    [[nodiscard]] bool isSource(std::string const& data_key) const;
    
    /**
     * @brief Get source keys for a container's lineage
     * 
     * @param data_key Key of the derived container
     * @return Vector of source data keys (empty if no lineage or Source type)
     */
    [[nodiscard]] std::vector<std::string> getSourceKeys(std::string const& data_key) const;
    
    /**
     * @brief Get all containers that depend on a given source
     * 
     * @param source_key Key of the source container
     * @return Vector of derived container keys
     */
    [[nodiscard]] std::vector<std::string> getDependentKeys(std::string const& source_key) const;
    
    /**
     * @brief Get the complete lineage chain from a container to its root sources
     * 
     * @param data_key Key of the starting container
     * @return Vector of data keys in order from the given key to root sources
     *         (may branch if there are multiple sources)
     */
    [[nodiscard]] std::vector<std::string> getLineageChain(std::string const& data_key) const;
    
    /**
     * @brief Get all registered data keys
     */
    [[nodiscard]] std::vector<std::string> getAllKeys() const;
    
    /**
     * @brief Get total number of registered lineages
     */
    [[nodiscard]] std::size_t size() const { return _lineages.size(); }
    
    /**
     * @brief Check if registry is empty
     */
    [[nodiscard]] bool empty() const { return _lineages.empty(); }
    
    // ========== Staleness ==========
    
    /**
     * @brief Mark a lineage as stale (out of sync with source)
     */
    void markStale(std::string const& data_key);
    
    /**
     * @brief Mark a lineage as valid (in sync with source)
     */
    void markValid(std::string const& data_key);
    
    /**
     * @brief Check if a lineage is marked as stale
     * 
     * @return true if stale or no lineage exists
     */
    [[nodiscard]] bool isStale(std::string const& data_key) const;
    
    /**
     * @brief Mark a lineage and all its dependents as stale
     * 
     * Recursively marks all containers that depend on this one as stale.
     */
    void propagateStale(std::string const& data_key);
    
    // ========== Invalidation Callback ==========
    
    /**
     * @brief Set a custom callback for invalidation events
     * 
     * The callback is invoked when markStale() is called.
     */
    void setInvalidationCallback(InvalidationCallback callback);
    
private:
    std::unordered_map<std::string, LineageEntry> _lineages;
    InvalidationCallback _invalidation_callback;
    
    /// Build reverse dependency map (source → dependents)
    [[nodiscard]] std::unordered_map<std::string, std::vector<std::string>> 
        buildDependencyMap() const;
};

} // namespace WhiskerToolbox::Lineage

#endif // WHISKERTOOLBOX_LINEAGE_REGISTRY_HPP
