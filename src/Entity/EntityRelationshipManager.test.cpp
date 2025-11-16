#include "Entity/EntityRelationshipManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <unordered_set>

TEST_CASE("EntityRelationshipManager - Basic Operations", "[entity][relationship][manager][basic]") {
    EntityRelationshipManager manager;
    
    EntityId entity1 = EntityId(100);
    EntityId entity2 = EntityId(200);
    EntityId entity3 = EntityId(300);
    
    SECTION("Add relationships") {
        REQUIRE(manager.addRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE(manager.addRelationship(entity1, entity3, RelationshipType::ParentChild));
        REQUIRE(manager.addRelationship(entity2, entity3, RelationshipType::Derived));
        
        // Try adding duplicate
        REQUIRE_FALSE(manager.addRelationship(entity1, entity2, RelationshipType::ParentChild));
        
        REQUIRE(manager.getRelationshipCount() == 3);
    }
    
    SECTION("Check relationship existence") {
        manager.addRelationship(entity1, entity2, RelationshipType::ParentChild);
        
        REQUIRE(manager.hasRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE_FALSE(manager.hasRelationship(entity1, entity2, RelationshipType::Derived));
        REQUIRE_FALSE(manager.hasRelationship(entity2, entity1, RelationshipType::ParentChild));
        REQUIRE_FALSE(manager.hasRelationship(entity1, entity3, RelationshipType::ParentChild));
    }
    
    SECTION("Remove relationships") {
        manager.addRelationship(entity1, entity2, RelationshipType::ParentChild);
        manager.addRelationship(entity1, entity3, RelationshipType::ParentChild);
        
        REQUIRE(manager.removeRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE_FALSE(manager.hasRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE(manager.hasRelationship(entity1, entity3, RelationshipType::ParentChild));
        
        // Try removing non-existent relationship
        REQUIRE_FALSE(manager.removeRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE_FALSE(manager.removeRelationship(entity2, entity3, RelationshipType::Derived));
        
        REQUIRE(manager.getRelationshipCount() == 1);
    }
    
    SECTION("Remove all relationships for an entity") {
        manager.addRelationship(entity1, entity2, RelationshipType::ParentChild);
        manager.addRelationship(entity1, entity3, RelationshipType::ParentChild);
        manager.addRelationship(entity2, entity3, RelationshipType::Derived);
        manager.addRelationship(entity3, entity1, RelationshipType::Linked);
        
        REQUIRE(manager.getRelationshipCount() == 4);
        
        // Remove all relationships involving entity1
        std::size_t removed = manager.removeAllRelationships(entity1);
        REQUIRE(removed == 3);  // entity1->entity2, entity1->entity3, entity3->entity1
        
        REQUIRE_FALSE(manager.hasRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE_FALSE(manager.hasRelationship(entity1, entity3, RelationshipType::ParentChild));
        REQUIRE_FALSE(manager.hasRelationship(entity3, entity1, RelationshipType::Linked));
        
        // entity2->entity3 should still exist
        REQUIRE(manager.hasRelationship(entity2, entity3, RelationshipType::Derived));
        REQUIRE(manager.getRelationshipCount() == 1);
    }
}

TEST_CASE("EntityRelationshipManager - Querying Relationships", "[entity][relationship][manager][query]") {
    EntityRelationshipManager manager;
    
    EntityId parent = EntityId(100);
    EntityId child1 = EntityId(200);
    EntityId child2 = EntityId(300);
    EntityId child3 = EntityId(400);
    
    SECTION("Get related entities (forward)") {
        manager.addRelationship(parent, child1, RelationshipType::ParentChild);
        manager.addRelationship(parent, child2, RelationshipType::ParentChild);
        manager.addRelationship(parent, child3, RelationshipType::Derived);
        
        // Get all related entities
        auto all_related = manager.getRelatedEntities(parent);
        REQUIRE(all_related.size() == 3);
        
        std::unordered_set<EntityId> related_set(all_related.begin(), all_related.end());
        REQUIRE(related_set.count(child1) == 1);
        REQUIRE(related_set.count(child2) == 1);
        REQUIRE(related_set.count(child3) == 1);
        
        // Get only ParentChild relationships
        auto parent_child_related = manager.getRelatedEntities(parent, RelationshipType::ParentChild);
        REQUIRE(parent_child_related.size() == 2);
        
        std::unordered_set<EntityId> pc_set(parent_child_related.begin(), parent_child_related.end());
        REQUIRE(pc_set.count(child1) == 1);
        REQUIRE(pc_set.count(child2) == 1);
        REQUIRE(pc_set.count(child3) == 0);
        
        // Get only Derived relationships
        auto derived_related = manager.getRelatedEntities(parent, RelationshipType::Derived);
        REQUIRE(derived_related.size() == 1);
        REQUIRE(derived_related[0] == child3);
        
        // Entity with no relationships
        auto empty = manager.getRelatedEntities(child1);
        REQUIRE(empty.empty());
    }
    
    SECTION("Get reverse related entities") {
        manager.addRelationship(parent, child1, RelationshipType::ParentChild);
        manager.addRelationship(child2, child1, RelationshipType::Derived);
        manager.addRelationship(child3, child1, RelationshipType::Linked);
        
        // Get all reverse related entities (entities pointing to child1)
        auto all_reverse = manager.getReverseRelatedEntities(child1);
        REQUIRE(all_reverse.size() == 3);
        
        std::unordered_set<EntityId> reverse_set(all_reverse.begin(), all_reverse.end());
        REQUIRE(reverse_set.count(parent) == 1);
        REQUIRE(reverse_set.count(child2) == 1);
        REQUIRE(reverse_set.count(child3) == 1);
        
        // Get only ParentChild reverse relationships
        auto pc_reverse = manager.getReverseRelatedEntities(child1, RelationshipType::ParentChild);
        REQUIRE(pc_reverse.size() == 1);
        REQUIRE(pc_reverse[0] == parent);
    }
}

TEST_CASE("EntityRelationshipManager - Parent-Child Convenience Methods", "[entity][relationship][manager][parent-child]") {
    EntityRelationshipManager manager;
    
    EntityId parent = EntityId(100);
    EntityId child1 = EntityId(200);
    EntityId child2 = EntityId(300);
    EntityId grandchild = EntityId(400);
    
    SECTION("Get children") {
        manager.addRelationship(parent, child1, RelationshipType::ParentChild);
        manager.addRelationship(parent, child2, RelationshipType::ParentChild);
        manager.addRelationship(parent, grandchild, RelationshipType::Derived);  // Not a child
        
        auto children = manager.getChildren(parent);
        REQUIRE(children.size() == 2);
        
        std::unordered_set<EntityId> child_set(children.begin(), children.end());
        REQUIRE(child_set.count(child1) == 1);
        REQUIRE(child_set.count(child2) == 1);
        REQUIRE(child_set.count(grandchild) == 0);
    }
    
    SECTION("Get parents") {
        manager.addRelationship(parent, child1, RelationshipType::ParentChild);
        manager.addRelationship(child1, grandchild, RelationshipType::ParentChild);
        manager.addRelationship(child2, grandchild, RelationshipType::Derived);  // Not a parent
        
        auto parents = manager.getParents(grandchild);
        REQUIRE(parents.size() == 1);
        REQUIRE(parents[0] == child1);
        
        auto child1_parents = manager.getParents(child1);
        REQUIRE(child1_parents.size() == 1);
        REQUIRE(child1_parents[0] == parent);
        
        auto parent_parents = manager.getParents(parent);
        REQUIRE(parent_parents.empty());
    }
    
    SECTION("Multi-parent scenario") {
        EntityId parent1 = EntityId(100);
        EntityId parent2 = EntityId(200);
        EntityId child = EntityId(300);
        
        manager.addRelationship(parent1, child, RelationshipType::ParentChild);
        manager.addRelationship(parent2, child, RelationshipType::ParentChild);
        
        auto parents = manager.getParents(child);
        REQUIRE(parents.size() == 2);
        
        std::unordered_set<EntityId> parent_set(parents.begin(), parents.end());
        REQUIRE(parent_set.count(parent1) == 1);
        REQUIRE(parent_set.count(parent2) == 1);
    }
}

TEST_CASE("EntityRelationshipManager - Relationship Details", "[entity][relationship][manager][details]") {
    EntityRelationshipManager manager;
    
    EntityId entity1 = EntityId(100);
    EntityId entity2 = EntityId(200);
    EntityId entity3 = EntityId(300);
    
    SECTION("Get relationship details with labels") {
        manager.addRelationship(entity1, entity2, RelationshipType::ParentChild, "mask to area");
        manager.addRelationship(entity1, entity3, RelationshipType::Derived, "processed output");
        
        auto details = manager.getRelationshipDetails(entity1, false);
        REQUIRE(details.size() == 2);
        
        // Find the ParentChild relationship
        auto pc_it = std::find_if(details.begin(), details.end(), [](EntityRelationship const & rel) {
            return rel.type == RelationshipType::ParentChild;
        });
        REQUIRE(pc_it != details.end());
        REQUIRE(pc_it->from_entity == entity1);
        REQUIRE(pc_it->to_entity == entity2);
        REQUIRE(pc_it->label == "mask to area");
        
        // Find the Derived relationship
        auto derived_it = std::find_if(details.begin(), details.end(), [](EntityRelationship const & rel) {
            return rel.type == RelationshipType::Derived;
        });
        REQUIRE(derived_it != details.end());
        REQUIRE(derived_it->from_entity == entity1);
        REQUIRE(derived_it->to_entity == entity3);
        REQUIRE(derived_it->label == "processed output");
    }
    
    SECTION("Get relationship details including reverse") {
        manager.addRelationship(entity1, entity2, RelationshipType::ParentChild);
        manager.addRelationship(entity3, entity1, RelationshipType::Linked);
        
        // Without reverse relationships
        auto forward_only = manager.getRelationshipDetails(entity1, false);
        REQUIRE(forward_only.size() == 1);
        REQUIRE(forward_only[0].from_entity == entity1);
        REQUIRE(forward_only[0].to_entity == entity2);
        
        // With reverse relationships
        auto with_reverse = manager.getRelationshipDetails(entity1, true);
        REQUIRE(with_reverse.size() == 2);
        
        // Should contain both forward and reverse relationships
        bool has_forward = false;
        bool has_reverse = false;
        for (auto const & rel : with_reverse) {
            if (rel.from_entity == entity1 && rel.to_entity == entity2) {
                has_forward = true;
            }
            if (rel.from_entity == entity3 && rel.to_entity == entity1) {
                has_reverse = true;
            }
        }
        REQUIRE(has_forward);
        REQUIRE(has_reverse);
    }
}

TEST_CASE("EntityRelationshipManager - Statistics", "[entity][relationship][manager][stats]") {
    EntityRelationshipManager manager;
    
    EntityId entity1 = EntityId(100);
    EntityId entity2 = EntityId(200);
    EntityId entity3 = EntityId(300);
    EntityId entity4 = EntityId(400);
    
    SECTION("Initial state") {
        REQUIRE(manager.getRelationshipCount() == 0);
        REQUIRE(manager.getEntityCount() == 0);
    }
    
    SECTION("Count relationships") {
        manager.addRelationship(entity1, entity2, RelationshipType::ParentChild);
        manager.addRelationship(entity1, entity3, RelationshipType::ParentChild);
        manager.addRelationship(entity2, entity4, RelationshipType::Derived);
        
        REQUIRE(manager.getRelationshipCount() == 3);
        REQUIRE(manager.getEntityCount() == 4);
        
        manager.removeRelationship(entity1, entity2, RelationshipType::ParentChild);
        REQUIRE(manager.getRelationshipCount() == 2);
        REQUIRE(manager.getEntityCount() == 4);  // All entities still involved (entity2 still has entity2->entity4)
    }
    
    SECTION("Clear all") {
        manager.addRelationship(entity1, entity2, RelationshipType::ParentChild);
        manager.addRelationship(entity2, entity3, RelationshipType::Derived);
        
        REQUIRE(manager.getRelationshipCount() == 2);
        
        manager.clear();
        
        REQUIRE(manager.getRelationshipCount() == 0);
        REQUIRE(manager.getEntityCount() == 0);
        REQUIRE_FALSE(manager.hasRelationship(entity1, entity2, RelationshipType::ParentChild));
        
        // Should be able to add relationships after clear
        REQUIRE(manager.addRelationship(entity3, entity4, RelationshipType::Linked));
        REQUIRE(manager.getRelationshipCount() == 1);
    }
}

TEST_CASE("EntityRelationshipManager - Multiple Relationship Types", "[entity][relationship][manager][types]") {
    EntityRelationshipManager manager;
    
    EntityId entity1 = EntityId(100);
    EntityId entity2 = EntityId(200);
    
    SECTION("Same entities with different relationship types") {
        // Add same entity pair with different types
        REQUIRE(manager.addRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE(manager.addRelationship(entity1, entity2, RelationshipType::Derived));
        REQUIRE(manager.addRelationship(entity1, entity2, RelationshipType::Linked));
        
        REQUIRE(manager.getRelationshipCount() == 3);
        
        REQUIRE(manager.hasRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE(manager.hasRelationship(entity1, entity2, RelationshipType::Derived));
        REQUIRE(manager.hasRelationship(entity1, entity2, RelationshipType::Linked));
        
        // Remove one type
        manager.removeRelationship(entity1, entity2, RelationshipType::Derived);
        
        REQUIRE(manager.hasRelationship(entity1, entity2, RelationshipType::ParentChild));
        REQUIRE_FALSE(manager.hasRelationship(entity1, entity2, RelationshipType::Derived));
        REQUIRE(manager.hasRelationship(entity1, entity2, RelationshipType::Linked));
        REQUIRE(manager.getRelationshipCount() == 2);
    }
}

TEST_CASE("EntityRelationshipManager - Performance", "[entity][relationship][manager][performance]") {
    EntityRelationshipManager manager;
    
    SECTION("Large number of relationships") {
        const std::size_t num_parents = 100;
        const std::size_t num_children_per_parent = 100;
        
        // Create a tree-like structure
        for (std::size_t parent_idx = 0; parent_idx < num_parents; ++parent_idx) {
            EntityId parent = EntityId(parent_idx);
            for (std::size_t child_idx = 0; child_idx < num_children_per_parent; ++child_idx) {
                EntityId child = EntityId(num_parents + parent_idx * num_children_per_parent + child_idx);
                manager.addRelationship(parent, child, RelationshipType::ParentChild);
            }
        }
        
        REQUIRE(manager.getRelationshipCount() == num_parents * num_children_per_parent);
        
        // Query children of first parent
        EntityId first_parent = EntityId(0);
        auto children = manager.getChildren(first_parent);
        REQUIRE(children.size() == num_children_per_parent);
        
        // Query parents of a child
        EntityId some_child = EntityId(num_parents + 50);  // Child of parent 0
        auto parents = manager.getParents(some_child);
        REQUIRE(parents.size() == 1);
        REQUIRE(parents[0] == first_parent);
        
        // Remove all relationships for a parent
        std::size_t removed = manager.removeAllRelationships(first_parent);
        REQUIRE(removed == num_children_per_parent);
        REQUIRE(manager.getRelationshipCount() == (num_parents - 1) * num_children_per_parent);
    }
    
    SECTION("Dense cross-references") {
        const std::size_t num_entities = 50;
        
        // Create a mesh where each entity has relationships to all others
        for (std::size_t i = 0; i < num_entities; ++i) {
            for (std::size_t j = 0; j < num_entities; ++j) {
                if (i != j) {
                    manager.addRelationship(EntityId(i), EntityId(j), RelationshipType::Linked);
                }
            }
        }
        
        REQUIRE(manager.getRelationshipCount() == num_entities * (num_entities - 1));
        
        // Each entity should have (num_entities - 1) related entities
        auto related = manager.getRelatedEntities(EntityId(25));
        REQUIRE(related.size() == num_entities - 1);
        
        // Each entity should also have (num_entities - 1) reverse related entities
        auto reverse_related = manager.getReverseRelatedEntities(EntityId(25));
        REQUIRE(reverse_related.size() == num_entities - 1);
    }
}
