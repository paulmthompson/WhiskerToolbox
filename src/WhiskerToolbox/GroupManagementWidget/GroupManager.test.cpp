#include "GroupManager.hpp"
#include "DataManager/Entity/EntityGroupManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <unordered_set>
#include <vector>

TEST_CASE("GroupManager - Group CRUD", "[groupmanager][groups][crud]") {
    EntityGroupManager egm;
    GroupManager gm(&egm);

    SECTION("Create groups and retrieve") {
        int g1 = gm.createGroup(QString("Alpha"));
        int g2 = gm.createGroup(QString("Beta"));

        REQUIRE(g1 != g2);

        // Verify presence through EntityGroupManager
        REQUIRE(egm.hasGroup(static_cast<GroupId>(g1)));
        REQUIRE(egm.hasGroup(static_cast<GroupId>(g2)));

        // Verify GroupManager view
        auto groups = gm.getGroups();
        REQUIRE(groups.contains(g1));
        REQUIRE(groups.contains(g2));
        REQUIRE(groups[g1].name == QString("Alpha"));
        REQUIRE(groups[g2].name == QString("Beta"));

        // Member counts start at 0
        REQUIRE(gm.getGroupMemberCount(g1) == 0);
        REQUIRE(gm.getGroupMemberCount(g2) == 0);
    }

    SECTION("Rename and color updates") {
        int g = gm.createGroup(QString("OldName"));
        REQUIRE(gm.setGroupName(g, QString("NewName")));

        // Verify via descriptor in EntityGroupManager
        auto desc = egm.getGroupDescriptor(static_cast<GroupId>(g));
        REQUIRE(desc.has_value());
        REQUIRE(desc->name == std::string("NewName"));

        // Update color and verify round-trip
        QColor c(10, 20, 30);
        REQUIRE(gm.setGroupColor(g, c));
        auto group_opt = gm.getGroup(g);
        REQUIRE(group_opt.has_value());
        REQUIRE(group_opt->color == c);
    }

    SECTION("Remove groups") {
        int g = gm.createGroup(QString("ToRemove"));
        REQUIRE(gm.removeGroup(g));
        REQUIRE_FALSE(egm.hasGroup(static_cast<GroupId>(g)));

        // Removing again should fail
        REQUIRE_FALSE(gm.removeGroup(g));
    }
}

TEST_CASE("GroupManager - Assign and Remove Entities", "[groupmanager][entities]") {
    EntityGroupManager egm;
    GroupManager gm(&egm);

    int g = gm.createGroup(QString("Members"));
    GroupId gid = static_cast<GroupId>(g);

    SECTION("Assign unique entities") {
        std::unordered_set<EntityId> ids = {1, 2, 3, 4, 5};
        REQUIRE(gm.assignEntitiesToGroup(g, ids));

        REQUIRE(gm.getGroupMemberCount(g) == 5);
        REQUIRE(egm.getGroupSize(gid) == 5);

        // Assigning the same set again should add zero and return false
        REQUIRE_FALSE(gm.assignEntitiesToGroup(g, ids));
        REQUIRE(gm.getGroupMemberCount(g) == 5);
    }

    SECTION("Remove subset of entities") {
        std::unordered_set<EntityId> ids = {10, 20, 30, 40, 50};
        REQUIRE(gm.assignEntitiesToGroup(g, ids));
        REQUIRE(gm.getGroupMemberCount(g) == 5);

        std::unordered_set<EntityId> to_remove = {20, 40};
        REQUIRE(gm.removeEntitiesFromGroup(g, to_remove));
        REQUIRE(gm.getGroupMemberCount(g) == 3);
        REQUIRE(egm.getGroupSize(gid) == 3);

        // Removing non-members should return false (no changes)
        std::unordered_set<EntityId> not_in_group = {99, 100};
        REQUIRE_FALSE(gm.removeEntitiesFromGroup(g, not_in_group));
        REQUIRE(gm.getGroupMemberCount(g) == 3);
    }

    SECTION("Ungroup entities across multiple groups") {
        int g2 = gm.createGroup(QString("Other"));
        GroupId gid2 = static_cast<GroupId>(g2);

        // Entity 7 belongs to both groups; 8 only to g
        std::unordered_set<EntityId> ids_g = {7, 8};
        std::unordered_set<EntityId> ids_g2 = {7};
        REQUIRE(gm.assignEntitiesToGroup(g, ids_g));
        REQUIRE(gm.assignEntitiesToGroup(g2, ids_g2));

        REQUIRE(egm.isEntityInGroup(gid, 7));
        REQUIRE(egm.isEntityInGroup(gid2, 7));
        REQUIRE(egm.isEntityInGroup(gid, 8));

        // Ungroup entity 7 from all groups; 8 remains in g
        gm.ungroupEntities({7});

        REQUIRE_FALSE(egm.isEntityInGroup(gid, 7));
        REQUIRE_FALSE(egm.isEntityInGroup(gid2, 7));
        REQUIRE(egm.isEntityInGroup(gid, 8));
    }
}
