#include "GroupManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "DataManager/DataManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <unordered_set>
#include <vector>
#include <QSignalSpy>

TEST_CASE("GroupManager - Group CRUD", "[groupmanager][groups][crud]") {
    EntityGroupManager egm;
    auto dm = std::make_shared<DataManager>();
    GroupManager gm(&egm, dm);

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
    auto dm = std::make_shared<DataManager>();
    GroupManager gm(&egm, dm);

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

TEST_CASE("GroupManager - Group Visibility", "[groupmanager][visibility]") {
    EntityGroupManager egm;
    auto dm = std::make_shared<DataManager>();
    GroupManager gm(&egm, dm);

    int g = gm.createGroup(QString("VisibleGroup"));
    GroupId gid = static_cast<GroupId>(g);

    SECTION("Default visibility is true") {
        auto group = gm.getGroup(g);
        REQUIRE(group.has_value());
        REQUIRE(group->visible == true);
    }

    SECTION("Set group visibility") {
        REQUIRE(gm.setGroupVisibility(g, false));
        
        auto group = gm.getGroup(g);
        REQUIRE(group.has_value());
        REQUIRE(group->visible == false);
        
        REQUIRE(gm.setGroupVisibility(g, true));
        
        group = gm.getGroup(g);
        REQUIRE(group.has_value());
        REQUIRE(group->visible == true);
    }

    SECTION("Entity group visibility check") {
        std::unordered_set<EntityId> ids = {1, 2, 3};
        REQUIRE(gm.assignEntitiesToGroup(g, ids));
        
        // Group is visible by default
        REQUIRE(gm.isEntityGroupVisible(1) == true);
        REQUIRE(gm.isEntityGroupVisible(2) == true);
        REQUIRE(gm.isEntityGroupVisible(3) == true);
        
        // Hide the group
        REQUIRE(gm.setGroupVisibility(g, false));
        REQUIRE(gm.isEntityGroupVisible(1) == false);
        REQUIRE(gm.isEntityGroupVisible(2) == false);
        REQUIRE(gm.isEntityGroupVisible(3) == false);
        
        // Show the group again
        REQUIRE(gm.setGroupVisibility(g, true));
        REQUIRE(gm.isEntityGroupVisible(1) == true);
        REQUIRE(gm.isEntityGroupVisible(2) == true);
        REQUIRE(gm.isEntityGroupVisible(3) == true);
    }

    SECTION("Entities not in groups are always visible") {
        // Entity 999 is not in any group
        REQUIRE(gm.isEntityGroupVisible(999) == true);
    }

    SECTION("Set visibility on non-existent group fails") {
        REQUIRE_FALSE(gm.setGroupVisibility(999, false));
    }
}

TEST_CASE("GroupManager - Signals emit once per logical change", "[groupmanager][signals]") {
    EntityGroupManager egm;
    auto dm = std::make_shared<DataManager>();
    GroupManager gm(&egm, dm);

    int g = gm.createGroup(QString("G1"));
    REQUIRE(g >= 0);

    SECTION("setGroupName unchanged does not emit groupModified; change emits once") {
        QSignalSpy modSpy(&gm, &GroupManager::groupModified);
        REQUIRE(gm.setGroupName(g, QString("G1"))); // unchanged
        REQUIRE(modSpy.count() == 0);
        REQUIRE(gm.setGroupName(g, QString("G2"))); // changed
        REQUIRE(modSpy.count() == 1);
    }

    SECTION("assignEntitiesToGroup emits once on first add, none on re-add") {
        QSignalSpy modSpy(&gm, &GroupManager::groupModified);
        std::unordered_set<EntityId> ids = {101, 102};
        REQUIRE(gm.assignEntitiesToGroup(g, ids) == true);
        REQUIRE(modSpy.count() == 1);
        // Re-assign same entities -> no change
        REQUIRE(gm.assignEntitiesToGroup(g, ids) == false);
        REQUIRE(modSpy.count() == 1);
    }

    SECTION("removeEntitiesFromGroup emits once when removing present members only") {
        std::unordered_set<EntityId> ids = {201, 202, 203};
        REQUIRE(gm.assignEntitiesToGroup(g, ids));
        QSignalSpy modSpy(&gm, &GroupManager::groupModified);
        REQUIRE(gm.removeEntitiesFromGroup(g, std::unordered_set<EntityId>{202}) == true);
        REQUIRE(modSpy.count() == 1);
        // Removing non-members -> no emission
        REQUIRE(gm.removeEntitiesFromGroup(g, std::unordered_set<EntityId>{999, 1000}) == false);
        REQUIRE(modSpy.count() == 1);
    }
}
