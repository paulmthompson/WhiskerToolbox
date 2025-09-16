#include "GroupManagementWidget.hpp"
#include "DataManager/Entity/EntityGroupManager.hpp"
#include "GroupManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QTableWidget>
#include <optional>
#include <unordered_set>

namespace {
// Match TableDesignerWidget test pattern: own a QApplication via fixture to avoid exit-order issues
class GroupManagementWidgetQtFixture {
protected:
    GroupManagementWidgetQtFixture() {
        if (!QApplication::instance()) {
            qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
            int argc = 0;
            char ** argv = nullptr;
            app = std::make_unique<QApplication>(argc, argv);
        }
    }

    ~GroupManagementWidgetQtFixture() = default;

private:
    std::unique_ptr<QApplication> app;
};

QTableWidget * findGroupsTable(GroupManagementWidget & widget) {
    auto * table = widget.findChild<QTableWidget *>("groupsTable");
    REQUIRE(table != nullptr);
    return table;
}

int findRowForGroupId(QTableWidget * table, int group_id) {
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem * item = table->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == group_id) {
            return row;
        }
    }
    return -1;
}
}// namespace

TEST_CASE_METHOD(GroupManagementWidgetQtFixture, "GroupManagementWidget - Rows update on group CRUD", "[groupmanagementwidget][rows]") {
    EntityGroupManager egm;
    GroupManager gm(&egm);
    GroupManagementWidget widget(&gm);

    auto * table = findGroupsTable(widget);
    QCoreApplication::processEvents();
    REQUIRE(table->rowCount() == 0);

    int g1 = gm.createGroup(QString("A"));
    int g2 = gm.createGroup(QString("B"));
    QCoreApplication::processEvents();
    REQUIRE(table->rowCount() == 2);

    REQUIRE(gm.removeGroup(g1));
    QCoreApplication::processEvents();
    REQUIRE(table->rowCount() == 1);
}

TEST_CASE_METHOD(GroupManagementWidgetQtFixture, "GroupManagementWidget - Member counts update after assignments (via groupModified)", "[groupmanagementwidget][members]") {
    EntityGroupManager egm;
    GroupManager gm(&egm);
    GroupManagementWidget widget(&gm);
    auto * table = findGroupsTable(widget);

    int g = gm.createGroup(QString("M"));
    GroupId gid = static_cast<GroupId>(g);
    QCoreApplication::processEvents();
    REQUIRE(table->rowCount() == 1);

    // Assign entities using GroupManager API
    std::unordered_set<EntityId> ids = {1, 2, 3};
    REQUIRE(gm.assignEntitiesToGroup(g, ids));

    // Force UI refresh of member counts via a groupModified signal
    QColor c(12, 34, 56);
    REQUIRE(gm.setGroupColor(g, c));
    QCoreApplication::processEvents();

    int row = findRowForGroupId(table, g);
    REQUIRE(row >= 0);
    QTableWidgetItem * members_item = table->item(row, 2);
    REQUIRE(members_item != nullptr);
    REQUIRE(members_item->text().toInt() == 3);

    // Remove a subset
    std::unordered_set<EntityId> rem = {2};
    REQUIRE(gm.removeEntitiesFromGroup(g, rem));

    // Trigger another UI update through group rename
    REQUIRE(gm.setGroupName(g, QString("M2")));
    QCoreApplication::processEvents();

    row = findRowForGroupId(table, g);
    REQUIRE(row >= 0);
    members_item = table->item(row, 2);
    REQUIRE(members_item != nullptr);
    REQUIRE(members_item->text().toInt() == 2);
}

TEST_CASE_METHOD(GroupManagementWidgetQtFixture, "GroupManagementWidget - Members column updates on add/remove", "[groupmanagementwidget][members][signals]") {
    EntityGroupManager egm;
    GroupManager gm(&egm);
    GroupManagementWidget widget(&gm);
    auto * table = findGroupsTable(widget);

    int g = gm.createGroup(QString("G"));
    GroupId gid = static_cast<GroupId>(g);
    QCoreApplication::processEvents();
    REQUIRE(table->rowCount() == 1);

    int row = findRowForGroupId(table, g);
    REQUIRE(row >= 0);
    QTableWidgetItem * members_item = table->item(row, 2);
    REQUIRE(members_item != nullptr);
    REQUIRE(members_item->text().toInt() == 0);

    // Add two entities and expect UI to refresh via groupModified
    std::unordered_set<EntityId> ids = {11, 22};
    REQUIRE(gm.assignEntitiesToGroup(g, ids));
    QCoreApplication::processEvents();
    row = findRowForGroupId(table, g);
    REQUIRE(row >= 0);
    members_item = table->item(row, 2);
    REQUIRE(members_item->text().toInt() == 2);

    // Remove one entity and expect decrement
    std::unordered_set<EntityId> rem = {11};
    REQUIRE(gm.removeEntitiesFromGroup(g, rem));
    QCoreApplication::processEvents();
    row = findRowForGroupId(table, g);
    REQUIRE(row >= 0);
    members_item = table->item(row, 2);
    REQUIRE(members_item->text().toInt() == 1);
}
