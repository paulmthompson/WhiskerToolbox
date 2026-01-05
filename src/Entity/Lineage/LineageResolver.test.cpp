#include <catch2/catch_test_macros.hpp>
#include "Entity/Lineage/LineageResolver.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"
#include "Entity/Lineage/LineageTypes.hpp"

#include <map>
#include <utility>

namespace {

using namespace WhiskerToolbox::Entity::Lineage;
// EntityId is in global namespace, not in WhiskerToolbox

/**
 * @brief Mock data source for testing without DataManager
 *
 * Provides a simple in-memory implementation of IEntityDataSource
 * that can be configured for specific test scenarios.
 */
class MockEntityDataSource : public IEntityDataSource {
public:
    /**
     * @brief Set EntityIds for a specific container at a specific time
     *
     * @param key Container identifier
     * @param time Time frame index
     * @param ids EntityIds to store
     */
    void setEntityIds(std::string const & key, TimeFrameIndex time,
                      std::vector<EntityId> ids) {
        _entity_ids[{key, time.getValue()}] = std::move(ids);
    }

    /**
     * @brief Set all EntityIds for a container (across all times)
     *
     * @param key Container identifier
     * @param ids All EntityIds in the container
     */
    void setAllEntityIds(std::string const & key, std::unordered_set<EntityId> ids) {
        _all_entity_ids[key] = std::move(ids);
    }

    [[nodiscard]] std::vector<EntityId> getEntityIds(
            std::string const & data_key,
            TimeFrameIndex time,
            std::size_t local_index) const override {
        auto it = _entity_ids.find({data_key, time.getValue()});
        if (it == _entity_ids.end()) {
            return {};
        }
        if (local_index >= it->second.size()) {
            return {};
        }
        return {it->second[local_index]};
    }

    [[nodiscard]] std::vector<EntityId> getAllEntityIdsAtTime(
            std::string const & data_key,
            TimeFrameIndex time) const override {
        auto it = _entity_ids.find({data_key, time.getValue()});
        if (it == _entity_ids.end()) {
            return {};
        }
        return it->second;
    }

    [[nodiscard]] std::unordered_set<EntityId> getAllEntityIds(
            std::string const & data_key) const override {
        auto it = _all_entity_ids.find(data_key);
        if (it == _all_entity_ids.end()) {
            return {};
        }
        return it->second;
    }

    [[nodiscard]] std::size_t getElementCount(
            std::string const & data_key,
            TimeFrameIndex time) const override {
        auto it = _entity_ids.find({data_key, time.getValue()});
        if (it == _entity_ids.end()) {
            return 0;
        }
        return it->second.size();
    }

private:
    std::map<std::pair<std::string, int64_t>, std::vector<EntityId>> _entity_ids;
    std::map<std::string, std::unordered_set<EntityId>> _all_entity_ids;
};

}// anonymous namespace

TEST_CASE("LineageResolver construction", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    SECTION("Can construct with valid pointers") {
        LineageResolver resolver(&data_source, &registry);
        REQUIRE_FALSE(resolver.hasLineage("nonexistent"));
    }

    SECTION("Can construct with null data source") {
        LineageResolver resolver(nullptr, &registry);
        // Operations should return empty results
        auto ids = resolver.resolveToSource("any", TimeFrameIndex(0));
        REQUIRE(ids.empty());
    }

    SECTION("Can construct with null registry") {
        LineageResolver resolver(&data_source, nullptr);
        // Should return container's own EntityIds
        data_source.setEntityIds("container", TimeFrameIndex(5), {EntityId(100)});
        auto ids = resolver.resolveToSource("container", TimeFrameIndex(5));
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));
    }
}

TEST_CASE("LineageResolver with Source lineage", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: container with Source lineage (root data)
    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100), EntityId(101)});
    registry.setLineage("masks", Source{});

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToSource returns own EntityIds") {
        auto ids = resolver.resolveToSource("masks", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));

        ids = resolver.resolveToSource("masks", TimeFrameIndex(10), 1);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(101));
    }

    SECTION("resolveToRoot returns own EntityIds") {
        auto ids = resolver.resolveToRoot("masks", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));
    }

    SECTION("isSource returns true") {
        REQUIRE(resolver.isSource("masks"));
    }

    SECTION("hasLineage returns true") {
        REQUIRE(resolver.hasLineage("masks"));
    }
}

TEST_CASE("LineageResolver with OneToOneByTime lineage", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: masks as source, mask_areas derived 1:1
    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100), EntityId(101)});
    registry.setLineage("masks", Source{});
    registry.setLineage("mask_areas", OneToOneByTime{"masks"});

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToSource returns source EntityIds at same position") {
        auto ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));

        ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(10), 1);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(101));
    }

    SECTION("resolveToRoot traverses to source") {
        auto ids = resolver.resolveToRoot("mask_areas", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));
    }

    SECTION("isSource returns false for derived container") {
        REQUIRE_FALSE(resolver.isSource("mask_areas"));
    }

    SECTION("getLineageChain returns correct chain") {
        auto chain = resolver.getLineageChain("mask_areas");
        REQUIRE(chain.size() == 2);
        REQUIRE(chain[0] == "mask_areas");
        REQUIRE(chain[1] == "masks");
    }
}

TEST_CASE("LineageResolver with AllToOneByTime lineage", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: masks as source, total_area aggregates all masks at each time
    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100), EntityId(101), EntityId(102)});
    registry.setLineage("masks", Source{});
    registry.setLineage("total_area", AllToOneByTime{"masks"});

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToSource returns all EntityIds at time") {
        auto ids = resolver.resolveToSource("total_area", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 3);
        // Check all expected IDs are present
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(100)) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(101)) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(102)) != ids.end());
    }

    SECTION("resolveToRoot returns all source EntityIds") {
        auto ids = resolver.resolveToRoot("total_area", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 3);
    }
}

TEST_CASE("LineageResolver with SubsetLineage", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: masks as source, filtered_masks contains only subset
    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100), EntityId(101), EntityId(102)});
    registry.setLineage("masks", Source{});

    SubsetLineage subset_lineage;
    subset_lineage.source_key = "masks";
    subset_lineage.included_entities = {EntityId(100), EntityId(102)};// Only 100 and 102
    registry.setLineage("filtered_masks", subset_lineage);

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToSource filters by included set") {
        // EntityId 100 is in the included set
        auto ids = resolver.resolveToSource("filtered_masks", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));

        // EntityId 101 is NOT in the included set
        ids = resolver.resolveToSource("filtered_masks", TimeFrameIndex(10), 1);
        REQUIRE(ids.empty());

        // EntityId 102 is in the included set
        ids = resolver.resolveToSource("filtered_masks", TimeFrameIndex(10), 2);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(102));
    }
}

TEST_CASE("LineageResolver with MultiSourceLineage", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: multiple sources combine into derived
    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100)});
    data_source.setEntityIds("lines", TimeFrameIndex(10), {EntityId(200), EntityId(201)});
    registry.setLineage("masks", Source{});
    registry.setLineage("lines", Source{});

    MultiSourceLineage multi_lineage;
    multi_lineage.source_keys = {"masks", "lines"};
    multi_lineage.strategy = MultiSourceLineage::CombineStrategy::ZipByTime;
    registry.setLineage("combined", multi_lineage);

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToSource returns EntityIds from all sources") {
        auto ids = resolver.resolveToSource("combined", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 3);
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(100)) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(200)) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(201)) != ids.end());
    }
}

TEST_CASE("LineageResolver with ExplicitLineage", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: explicit mapping of which source entities contributed to each derived element
    ExplicitLineage explicit_lineage;
    explicit_lineage.source_key = "events";
    explicit_lineage.contributors = {
            {EntityId(400), EntityId(401), EntityId(402)},// derived[0] came from these
            {EntityId(403)},                              // derived[1] came from this
            {EntityId(404), EntityId(405)}                // derived[2] came from these
    };
    registry.setLineage("intervals", explicit_lineage);

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToSource returns explicit contributors") {
        auto ids = resolver.resolveToSource("intervals", TimeFrameIndex(0), 0);
        REQUIRE(ids.size() == 3);
        REQUIRE(ids[0] == EntityId(400));
        REQUIRE(ids[1] == EntityId(401));
        REQUIRE(ids[2] == EntityId(402));

        ids = resolver.resolveToSource("intervals", TimeFrameIndex(0), 1);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(403));

        ids = resolver.resolveToSource("intervals", TimeFrameIndex(0), 2);
        REQUIRE(ids.size() == 2);
    }

    SECTION("resolveToSource returns empty for out-of-range index") {
        auto ids = resolver.resolveToSource("intervals", TimeFrameIndex(0), 100);
        REQUIRE(ids.empty());
    }
}

TEST_CASE("LineageResolver with EntityMappedLineage", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: entity-to-entity mapping (e.g., lines derived from masks)
    EntityMappedLineage mapped_lineage;
    mapped_lineage.source_key = "masks";
    mapped_lineage.entity_mapping = {
            {EntityId(200), {EntityId(100)}},        // Line 200 came from Mask 100
            {EntityId(201), {EntityId(100)}},        // Line 201 also came from Mask 100 (1:N)
            {EntityId(202), {EntityId(101), EntityId(102)}}// Line 202 came from Masks 101, 102 (N:1)
    };
    registry.setLineage("derived_lines", mapped_lineage);

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveByEntityId returns mapped parent EntityIds") {
        auto ids = resolver.resolveByEntityId("derived_lines", EntityId(200));
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));

        ids = resolver.resolveByEntityId("derived_lines", EntityId(201));
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));

        ids = resolver.resolveByEntityId("derived_lines", EntityId(202));
        REQUIRE(ids.size() == 2);
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(101)) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(102)) != ids.end());
    }

    SECTION("resolveByEntityId returns empty for unmapped EntityId") {
        auto ids = resolver.resolveByEntityId("derived_lines", EntityId(999));
        REQUIRE(ids.empty());
    }

    SECTION("resolveToSource returns empty for EntityMappedLineage") {
        // Position-based resolution doesn't work for EntityMappedLineage
        auto ids = resolver.resolveToSource("derived_lines", TimeFrameIndex(0), 0);
        REQUIRE(ids.empty());
    }
}

TEST_CASE("LineageResolver with ImplicitEntityMapping", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100), EntityId(101)});

    SECTION("OneToOne cardinality") {
        ImplicitEntityMapping implicit_lineage;
        implicit_lineage.source_key = "masks";
        implicit_lineage.cardinality = ImplicitEntityMapping::Cardinality::OneToOne;
        registry.setLineage("lines", implicit_lineage);

        LineageResolver resolver(&data_source, &registry);

        auto ids = resolver.resolveToSource("lines", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));

        ids = resolver.resolveToSource("lines", TimeFrameIndex(10), 1);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(101));
    }

    SECTION("AllToOne cardinality") {
        ImplicitEntityMapping implicit_lineage;
        implicit_lineage.source_key = "masks";
        implicit_lineage.cardinality = ImplicitEntityMapping::Cardinality::AllToOne;
        registry.setLineage("aggregate", implicit_lineage);

        LineageResolver resolver(&data_source, &registry);

        auto ids = resolver.resolveToSource("aggregate", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 2);
    }

    SECTION("OneToAll cardinality") {
        ImplicitEntityMapping implicit_lineage;
        implicit_lineage.source_key = "masks";
        implicit_lineage.cardinality = ImplicitEntityMapping::Cardinality::OneToAll;
        registry.setLineage("broadcast", implicit_lineage);

        LineageResolver resolver(&data_source, &registry);

        // All derived elements at time T map to source[T, 0]
        auto ids = resolver.resolveToSource("broadcast", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));

        // Even derived[1] maps to source[T, 0]
        ids = resolver.resolveToSource("broadcast", TimeFrameIndex(10), 1);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));
    }
}

TEST_CASE("LineageResolver chain resolution", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: masks → areas → peaks (3-level chain)
    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100), EntityId(101)});
    registry.setLineage("masks", Source{});
    registry.setLineage("areas", OneToOneByTime{"masks"});
    registry.setLineage("peaks", OneToOneByTime{"areas"});

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToSource goes one level") {
        // peaks → areas (not all the way to masks)
        auto ids = resolver.resolveToSource("peaks", TimeFrameIndex(10), 0);
        // This should return EntityIds from "areas" at (10, 0), which maps to "masks" at (10, 0)
        // But resolveToSource only goes one level, so it asks data_source for "areas" EntityIds
        // Since we didn't set EntityIds for "areas", it returns empty
        // In a real scenario, "areas" would have its own EntityIds stored
        REQUIRE(ids.empty());
    }

    SECTION("resolveToRoot traverses full chain") {
        // peaks → areas → masks
        auto ids = resolver.resolveToRoot("peaks", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));
    }

    SECTION("getLineageChain returns full chain") {
        auto chain = resolver.getLineageChain("peaks");
        REQUIRE(chain.size() == 3);
        REQUIRE(chain[0] == "peaks");
        REQUIRE(chain[1] == "areas");
        REQUIRE(chain[2] == "masks");
    }
}

TEST_CASE("LineageResolver with AllToOne chain resolution", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup: masks → areas (1:1) → total_area (N:1)
    // This tests the special case where AllToOneByTime needs to resolve through intermediate
    data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100), EntityId(101), EntityId(102)});
    data_source.setEntityIds("areas", TimeFrameIndex(10), {EntityId(200), EntityId(201), EntityId(202)});
    registry.setLineage("masks", Source{});
    registry.setLineage("areas", OneToOneByTime{"masks"});
    registry.setLineage("total_area", AllToOneByTime{"areas"});

    LineageResolver resolver(&data_source, &registry);

    SECTION("resolveToRoot through AllToOne finds all root entities") {
        auto ids = resolver.resolveToRoot("total_area", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 3);
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(100)) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(101)) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), EntityId(102)) != ids.end());
    }
}

TEST_CASE("LineageResolver getAllSourceEntities", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Setup source data
    std::unordered_set<EntityId> mask_entities = {EntityId(100), EntityId(101), EntityId(102)};
    data_source.setAllEntityIds("masks", mask_entities);
    registry.setLineage("masks", Source{});
    registry.setLineage("areas", OneToOneByTime{"masks"});

    LineageResolver resolver(&data_source, &registry);

    SECTION("Returns all entities for source container") {
        auto entities = resolver.getAllSourceEntities("masks");
        REQUIRE(entities.size() == 3);
        REQUIRE(entities.count(EntityId(100)) == 1);
        REQUIRE(entities.count(EntityId(101)) == 1);
        REQUIRE(entities.count(EntityId(102)) == 1);
    }

    SECTION("Returns source entities for derived container") {
        auto entities = resolver.getAllSourceEntities("areas");
        REQUIRE(entities.size() == 3);
    }
}

TEST_CASE("LineageResolver edge cases", "[entity][lineage][resolver]") {
    MockEntityDataSource data_source;
    LineageRegistry registry;

    LineageResolver resolver(&data_source, &registry);

    SECTION("Nonexistent container returns empty") {
        auto ids = resolver.resolveToSource("nonexistent", TimeFrameIndex(0));
        REQUIRE(ids.empty());
    }

    SECTION("Container without lineage uses own EntityIds") {
        data_source.setEntityIds("unregistered", TimeFrameIndex(5), {EntityId(999)});
        auto ids = resolver.resolveToSource("unregistered", TimeFrameIndex(5));
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(999));
    }

    SECTION("Out of range local_index returns empty") {
        data_source.setEntityIds("masks", TimeFrameIndex(10), {EntityId(100)});
        registry.setLineage("masks", Source{});

        auto ids = resolver.resolveToSource("masks", TimeFrameIndex(10), 100);
        REQUIRE(ids.empty());
    }

    SECTION("Empty time returns empty") {
        registry.setLineage("empty", Source{});
        auto ids = resolver.resolveToSource("empty", TimeFrameIndex(0));
        REQUIRE(ids.empty());
    }
}
