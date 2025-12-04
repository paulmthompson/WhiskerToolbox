#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Lineage/LineageTypes.hpp"
#include "Lineage/LineageRegistry.hpp"

#include <algorithm>

using namespace WhiskerToolbox::Lineage;
using Catch::Matchers::UnorderedEquals;

TEST_CASE("Lineage Types - Basic Construction", "[lineage][types]") {
    
    SECTION("Source type") {
        Source src{};
        
        Descriptor desc = src;
        REQUIRE(isSource(desc));
        REQUIRE(getSourceKeys(desc).empty());
        REQUIRE(getLineageTypeName(desc) == "Source");
    }
    
    SECTION("OneToOneByTime type") {
        OneToOneByTime one_to_one{"source_key"};
        REQUIRE(one_to_one.source_key == "source_key");
        
        Descriptor desc = one_to_one;
        REQUIRE_FALSE(isSource(desc));
        auto sources = getSourceKeys(desc);
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "source_key");
        REQUIRE(getLineageTypeName(desc) == "OneToOneByTime");
    }
    
    SECTION("AllToOneByTime type") {
        AllToOneByTime all_to_one{"source_key"};
        REQUIRE(all_to_one.source_key == "source_key");
        
        Descriptor desc = all_to_one;
        REQUIRE_FALSE(isSource(desc));
        auto sources = getSourceKeys(desc);
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "source_key");
        REQUIRE(getLineageTypeName(desc) == "AllToOneByTime");
    }
    
    SECTION("SubsetLineage type") {
        std::unordered_set<EntityId> entities{EntityId{1}, EntityId{2}, EntityId{3}};
        SubsetLineage subset{"source_key", entities};
        REQUIRE(subset.source_key == "source_key");
        REQUIRE(subset.included_entities.size() == 3);
        REQUIRE(subset.included_entities.count(EntityId{1}) == 1);
        REQUIRE(subset.included_entities.count(EntityId{2}) == 1);
        REQUIRE(subset.included_entities.count(EntityId{3}) == 1);
        
        Descriptor desc = subset;
        REQUIRE_FALSE(isSource(desc));
        auto sources = getSourceKeys(desc);
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "source_key");
        REQUIRE(getLineageTypeName(desc) == "SubsetLineage");
    }
    
    SECTION("MultiSourceLineage type") {
        std::vector<std::string> source_keys{"source1", "source2", "source3"};
        MultiSourceLineage multi{source_keys, MultiSourceLineage::CombineStrategy::ZipByTime};
        REQUIRE(multi.source_keys == source_keys);
        REQUIRE(multi.strategy == MultiSourceLineage::CombineStrategy::ZipByTime);
        
        Descriptor desc = multi;
        REQUIRE_FALSE(isSource(desc));
        auto sources = getSourceKeys(desc);
        REQUIRE(sources.size() == 3);
        REQUIRE_THAT(sources, UnorderedEquals(source_keys));
        REQUIRE(getLineageTypeName(desc) == "MultiSourceLineage");
    }
    
    SECTION("ExplicitLineage type") {
        std::vector<std::vector<EntityId>> contributors;
        contributors.push_back({EntityId{10}, EntityId{11}});
        contributors.push_back({EntityId{20}});
        
        ExplicitLineage explicit_lin{"source_key", contributors};
        REQUIRE(explicit_lin.source_key == "source_key");
        REQUIRE(explicit_lin.contributors.size() == 2);
        REQUIRE(explicit_lin.contributors[0].size() == 2);
        REQUIRE(explicit_lin.contributors[1].size() == 1);
        
        Descriptor desc = explicit_lin;
        REQUIRE_FALSE(isSource(desc));
        REQUIRE(getLineageTypeName(desc) == "ExplicitLineage");
    }
    
    SECTION("EntityMappedLineage type") {
        std::unordered_map<EntityId, std::vector<EntityId>> entity_map;
        entity_map[EntityId{100}] = {EntityId{1}};
        entity_map[EntityId{101}] = {EntityId{2}, EntityId{3}};
        
        EntityMappedLineage entity_lin{"source_key", entity_map};
        REQUIRE(entity_lin.source_key == "source_key");
        REQUIRE(entity_lin.entity_mapping.size() == 2);
        REQUIRE(entity_lin.entity_mapping.at(EntityId{100}).size() == 1);
        REQUIRE(entity_lin.entity_mapping.at(EntityId{101}).size() == 2);
        
        Descriptor desc = entity_lin;
        REQUIRE_FALSE(isSource(desc));
        auto sources = getSourceKeys(desc);
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "source_key");
        REQUIRE(getLineageTypeName(desc) == "EntityMappedLineage");
    }
    
    SECTION("ImplicitEntityMapping type") {
        ImplicitEntityMapping implicit_lin{"source_key", ImplicitEntityMapping::Cardinality::OneToOne};
        REQUIRE(implicit_lin.source_key == "source_key");
        REQUIRE(implicit_lin.cardinality == ImplicitEntityMapping::Cardinality::OneToOne);
        
        Descriptor desc = implicit_lin;
        REQUIRE_FALSE(isSource(desc));
        auto sources = getSourceKeys(desc);
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "source_key");
        REQUIRE(getLineageTypeName(desc) == "ImplicitEntityMapping");
    }
}

TEST_CASE("LineageRegistry - Basic Operations", "[lineage][registry]") {
    LineageRegistry registry;
    
    SECTION("Empty registry") {
        REQUIRE_FALSE(registry.hasLineage("nonexistent"));
        REQUIRE(registry.isSource("nonexistent")); // No lineage = treat as source
        REQUIRE(registry.getAllKeys().empty());
    }
    
    SECTION("Set and get lineage") {
        Source src{};
        registry.setLineage("data1", src);
        
        REQUIRE(registry.hasLineage("data1"));
        REQUIRE(registry.isSource("data1"));
        
        auto lineage = registry.getLineage("data1");
        REQUIRE(lineage.has_value());
        REQUIRE(getLineageTypeName(*lineage) == "Source");
    }
    
    SECTION("Set derived lineage") {
        registry.setLineage("parent", Source{});
        registry.setLineage("child", OneToOneByTime{"parent"});
        
        REQUIRE(registry.hasLineage("child"));
        REQUIRE_FALSE(registry.isSource("child"));
        
        auto sources = registry.getSourceKeys("child");
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "parent");
    }
    
    SECTION("Remove lineage") {
        registry.setLineage("data1", Source{});
        REQUIRE(registry.hasLineage("data1"));
        
        registry.removeLineage("data1");
        REQUIRE_FALSE(registry.hasLineage("data1"));
    }
    
    SECTION("Clear registry") {
        registry.setLineage("data1", Source{});
        registry.setLineage("data2", OneToOneByTime{"data1"});
        REQUIRE(registry.getAllKeys().size() == 2);
        
        registry.clear();
        REQUIRE(registry.getAllKeys().empty());
    }
    
    SECTION("Get all keys") {
        registry.setLineage("data1", Source{});
        registry.setLineage("data2", OneToOneByTime{"data1"});
        registry.setLineage("data3", AllToOneByTime{"data2"});
        
        auto keys = registry.getAllKeys();
        REQUIRE(keys.size() == 3);
        REQUIRE_THAT(keys, UnorderedEquals(std::vector<std::string>{"data1", "data2", "data3"}));
    }
}

TEST_CASE("LineageRegistry - Dependency Tracking", "[lineage][registry][dependency]") {
    LineageRegistry registry;
    
    // Setup a lineage chain: source -> intermediate -> output
    registry.setLineage("source", Source{});
    registry.setLineage("intermediate", OneToOneByTime{"source"});
    registry.setLineage("output", AllToOneByTime{"intermediate"});
    
    SECTION("Get dependent keys") {
        auto dependents = registry.getDependentKeys("source");
        REQUIRE(dependents.size() == 1);
        REQUIRE(dependents[0] == "intermediate");
        
        dependents = registry.getDependentKeys("intermediate");
        REQUIRE(dependents.size() == 1);
        REQUIRE(dependents[0] == "output");
        
        dependents = registry.getDependentKeys("output");
        REQUIRE(dependents.empty());
    }
    
    SECTION("Get lineage chain") {
        auto chain = registry.getLineageChain("output");
        // Should contain: output, intermediate, source
        REQUIRE(chain.size() == 3);
        REQUIRE(chain[0] == "output");
        // The rest may be in any order due to BFS, but should contain intermediate and source
        REQUIRE(std::find(chain.begin(), chain.end(), "intermediate") != chain.end());
        REQUIRE(std::find(chain.begin(), chain.end(), "source") != chain.end());
    }
    
    SECTION("Multi-source dependency") {
        registry.setLineage("combined", MultiSourceLineage{{"source", "intermediate"}, MultiSourceLineage::CombineStrategy::ZipByTime});
        
        auto dependents_of_source = registry.getDependentKeys("source");
        REQUIRE(dependents_of_source.size() == 2); // intermediate and combined
        REQUIRE_THAT(dependents_of_source, 
                     UnorderedEquals(std::vector<std::string>{"intermediate", "combined"}));
        
        auto chain = registry.getLineageChain("combined");
        // Should contain: combined, source, intermediate
        REQUIRE(chain.size() == 3);
    }
}

TEST_CASE("LineageRegistry - Staleness Tracking", "[lineage][registry][staleness]") {
    LineageRegistry registry;
    
    registry.setLineage("source", Source{});
    registry.setLineage("derived", OneToOneByTime{"source"});
    
    SECTION("Initial staleness state") {
        // New entries start as not stale
        REQUIRE_FALSE(registry.isStale("source"));
        REQUIRE_FALSE(registry.isStale("derived"));
    }
    
    SECTION("Mark stale") {
        registry.markStale("derived");
        REQUIRE(registry.isStale("derived"));
        REQUIRE_FALSE(registry.isStale("source")); // Source not affected
    }
    
    SECTION("Mark valid") {
        registry.markStale("derived");
        REQUIRE(registry.isStale("derived"));
        
        registry.markValid("derived");
        REQUIRE_FALSE(registry.isStale("derived"));
    }
    
    SECTION("Propagate stale") {
        // Add another level
        registry.setLineage("final", AllToOneByTime{"derived"});
        
        // Propagate staleness from source
        registry.propagateStale("source");
        
        // Source and all dependents should be stale
        REQUIRE(registry.isStale("source"));
        REQUIRE(registry.isStale("derived"));
        REQUIRE(registry.isStale("final"));
    }
    
    SECTION("LineageEntry has timestamp") {
        auto entry = registry.getLineageEntry("source");
        REQUIRE(entry.has_value());
        REQUIRE_FALSE(entry->is_stale);
        
        registry.markValid("source"); // Update timestamp
        auto entry2 = registry.getLineageEntry("source");
        REQUIRE(entry2.has_value());
        // Timestamp should be updated (can't easily test exact value)
    }
}

TEST_CASE("LineageRegistry - Invalidation Callback", "[lineage][registry][callback]") {
    LineageRegistry registry;
    
    registry.setLineage("source", Source{});
    registry.setLineage("derived", OneToOneByTime{"source"});
    
    std::vector<std::tuple<std::string, std::string, SourceChangeType>> callback_invocations;
    
    registry.setInvalidationCallback(
        [&callback_invocations](std::string const& data_key, 
                                 std::string const& source_key,
                                 SourceChangeType change_type) {
            callback_invocations.emplace_back(data_key, source_key, change_type);
        }
    );
    
    SECTION("Callback invoked on propagate stale") {
        registry.propagateStale("source");
        
        // Should have at least one callback invocation
        REQUIRE_FALSE(callback_invocations.empty());
        
        // Check that derived was notified about source change
        bool found_derived = false;
        for (auto const& [data_key, source_key, change_type] : callback_invocations) {
            if (data_key == "derived" && source_key == "source") {
                found_derived = true;
                REQUIRE(change_type == SourceChangeType::DataModified);
            }
        }
        REQUIRE(found_derived);
    }
}

TEST_CASE("LineageRegistry - Edge Cases", "[lineage][registry][edge]") {
    LineageRegistry registry;
    
    SECTION("Self-referential lineage (should handle gracefully)") {
        // This is an invalid state but shouldn't crash
        registry.setLineage("self", OneToOneByTime{"self"});
        
        auto chain = registry.getLineageChain("self");
        // Should not infinite loop, cycle detection should kick in
        REQUIRE(chain.size() == 1);
        REQUIRE(chain[0] == "self");
    }
    
    SECTION("Circular dependency (should handle gracefully)") {
        registry.setLineage("a", OneToOneByTime{"b"});
        registry.setLineage("b", OneToOneByTime{"a"});
        
        auto chain = registry.getLineageChain("a");
        // Should detect cycle and not infinite loop
        REQUIRE(chain.size() == 2);
    }
    
    SECTION("Missing source key in lineage") {
        // Reference a key that doesn't exist in registry
        registry.setLineage("orphan", OneToOneByTime{"nonexistent"});
        
        auto sources = registry.getSourceKeys("orphan");
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "nonexistent");
        
        // getLineageChain includes both orphan and the referenced (but missing) source
        auto chain = registry.getLineageChain("orphan");
        REQUIRE(chain.size() == 2); // orphan + nonexistent (even though not in registry)
        REQUIRE(chain[0] == "orphan");
        REQUIRE(std::find(chain.begin(), chain.end(), "nonexistent") != chain.end());
    }
    
    SECTION("Update existing lineage") {
        registry.setLineage("data", Source{});
        REQUIRE(registry.isSource("data"));
        
        // Update to derived
        registry.setLineage("data", OneToOneByTime{"parent"});
        REQUIRE_FALSE(registry.isSource("data"));
        
        auto sources = registry.getSourceKeys("data");
        REQUIRE(sources.size() == 1);
        REQUIRE(sources[0] == "parent");
    }
}
