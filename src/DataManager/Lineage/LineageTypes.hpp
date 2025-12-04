#ifndef WHISKERTOOLBOX_LINEAGE_TYPES_HPP
#define WHISKERTOOLBOX_LINEAGE_TYPES_HPP

#include "Entity/EntityTypes.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace WhiskerToolbox::Lineage {

/**
 * @brief No lineage - this is source data or data loaded from file
 * 
 * Use this for containers that are the original source of data,
 * not derived from any other container.
 */
struct Source {};

/**
 * @brief 1:1 mapping by time: derived[time, idx] ← source[time, idx]
 * 
 * Each element in the derived container corresponds to exactly one
 * element in the source container at the same time and local index.
 * 
 * Example: MaskData → AnalogTimeSeries via MaskArea transform
 * - mask_areas[T0] came from masks[T0]
 * - Resolution: Look up source EntityIds at the same TimeFrameIndex
 */
struct OneToOneByTime {
    std::string source_key;
};

/**
 * @brief N:1 mapping: derived[time] ← ALL source entities at time
 * 
 * Each element in the derived container is computed from all elements
 * in the source container at that time (reduction/aggregation).
 * 
 * Example: Sum of all mask areas at each time
 * - total_area[T0] came from ALL masks at T0
 * - Resolution: Return all source EntityIds at that time
 */
struct AllToOneByTime {
    std::string source_key;
};

/**
 * @brief Subset mapping: derived came from specific subset of source
 * 
 * The derived container contains only elements that correspond to
 * a specific subset of source entities.
 * 
 * Example: Filtered mask areas (only masks with area < 50)
 * - small_areas contains only values from masks {E1, E3, E7}
 * - Resolution: Return intersection of source EntityIds and included set
 */
struct SubsetLineage {
    std::string source_key;
    std::unordered_set<EntityId> included_entities;
    
    /// Optional: key of intermediate container this was filtered from
    std::optional<std::string> filtered_from_key;
};

/**
 * @brief Multi-source: derived from multiple parent containers
 * 
 * The derived container combines data from multiple source containers.
 * 
 * Example: Line distance computed from LineData and PointData
 */
struct MultiSourceLineage {
    std::vector<std::string> source_keys;
    
    enum class CombineStrategy {
        ZipByTime,   ///< Match elements by TimeFrameIndex
        Cartesian,   ///< All combinations
        Custom       ///< Application-specific logic
    };
    CombineStrategy strategy = CombineStrategy::ZipByTime;
};

/**
 * @brief Explicit per-element contributors (highest flexibility, highest storage)
 * 
 * For complex transformations where each derived element has a specific
 * set of source entities that contributed to it.
 * 
 * Example: Event intervals gathered from multiple source events
 * - interval[0] came from events {E400, E401, E402}
 * - interval[1] came from events {E403}
 */
struct ExplicitLineage {
    std::string source_key;
    
    /// contributors[derived_idx] = vector of source EntityIds
    std::vector<std::vector<EntityId>> contributors;
};

/**
 * @brief Explicit entity-to-entity mapping for entity-bearing derived containers
 * 
 * When both source and derived containers have their own EntityIds,
 * this maps from derived EntityId to parent EntityId(s).
 * 
 * Example: LineData from MaskData via skeletonization
 * - Line EntityId=200 came from Mask EntityId=100
 * - Line EntityId=201 came from Mask EntityId=100 (1:N case)
 * 
 * For 1:1: each derived EntityId maps to exactly one parent
 * For N:1: one derived EntityId maps to multiple parents
 * For 1:N: multiple derived EntityIds map to the same parent
 */
struct EntityMappedLineage {
    std::string source_key;
    
    /// derived_entity_id → [parent_entity_ids]
    std::unordered_map<EntityId, std::vector<EntityId>> entity_mapping;
};

/**
 * @brief Implicit entity mapping (computed on demand, no storage)
 * 
 * When both containers have the same time structure, the mapping
 * can be computed implicitly based on position.
 * 
 * Example: LineData from MaskData (1:1 transform)
 * - Line at (T0, idx=0) came from Mask at (T0, idx=0)
 * - No explicit storage needed
 */
struct ImplicitEntityMapping {
    std::string source_key;
    
    enum class Cardinality {
        OneToOne,   ///< derived[time, i] ← source[time, i]
        AllToOne,   ///< derived[time, 0] ← all source[time, *]
        OneToAll    ///< derived[time, *] ← source[time, 0]
    };
    Cardinality cardinality = Cardinality::OneToOne;
};

/**
 * @brief Type-erased lineage descriptor
 * 
 * Use std::visit to dispatch on the actual lineage type.
 */
using Descriptor = std::variant<
    Source,
    OneToOneByTime,
    AllToOneByTime,
    SubsetLineage,
    MultiSourceLineage,
    ExplicitLineage,
    EntityMappedLineage,
    ImplicitEntityMapping
>;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Check if the lineage descriptor represents source data (no parent)
 */
[[nodiscard]] inline bool isSource(Descriptor const& desc) {
    return std::holds_alternative<Source>(desc);
}

/**
 * @brief Get all source keys referenced by a lineage descriptor
 * 
 * @return Vector of source data keys (empty for Source type)
 */
[[nodiscard]] inline std::vector<std::string> getSourceKeys(Descriptor const& desc) {
    return std::visit([](auto const& lineage) -> std::vector<std::string> {
        using T = std::decay_t<decltype(lineage)>;
        
        if constexpr (std::is_same_v<T, Source>) {
            return {};
        } else if constexpr (std::is_same_v<T, MultiSourceLineage>) {
            return lineage.source_keys;
        } else {
            // All other types have a single source_key
            return {lineage.source_key};
        }
    }, desc);
}

/**
 * @brief Get a human-readable name for the lineage type
 */
[[nodiscard]] inline std::string getLineageTypeName(Descriptor const& desc) {
    return std::visit([](auto const& lineage) -> std::string {
        using T = std::decay_t<decltype(lineage)>;
        
        if constexpr (std::is_same_v<T, Source>) {
            return "Source";
        } else if constexpr (std::is_same_v<T, OneToOneByTime>) {
            return "OneToOneByTime";
        } else if constexpr (std::is_same_v<T, AllToOneByTime>) {
            return "AllToOneByTime";
        } else if constexpr (std::is_same_v<T, SubsetLineage>) {
            return "SubsetLineage";
        } else if constexpr (std::is_same_v<T, MultiSourceLineage>) {
            return "MultiSourceLineage";
        } else if constexpr (std::is_same_v<T, ExplicitLineage>) {
            return "ExplicitLineage";
        } else if constexpr (std::is_same_v<T, EntityMappedLineage>) {
            return "EntityMappedLineage";
        } else if constexpr (std::is_same_v<T, ImplicitEntityMapping>) {
            return "ImplicitEntityMapping";
        } else {
            return "Unknown";
        }
    }, desc);
}

} // namespace WhiskerToolbox::Lineage

#endif // WHISKERTOOLBOX_LINEAGE_TYPES_HPP
