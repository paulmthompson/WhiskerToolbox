#include <catch2/catch_test_macros.hpp>
#include <QSignalSpy>

#include "Analysis_Dashboard/Groups/GroupCoordinator.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "DataManager/Entity/EntityGroupManager.hpp"
#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"

#include "../fixtures/qt_test_fixtures.hpp"

class SpyPlot : public AbstractPlotWidget {
    Q_OBJECT
public:
    SpyPlot() : AbstractPlotWidget(nullptr) {}
    QString getPlotType() const override { return QStringLiteral("SpyPlot"); }
    void onGroupPropertiesChanged(int) override { emit propsChanged(); }
signals:
    void propsChanged();
};

TEST_CASE_METHOD(QtWidgetTestFixture, "GroupCoordinator forwards to multiple plots", "[GroupCoordinator][Signals]") {
    EntityGroupManager egm;
    GroupManager gm(&egm);
    GroupCoordinator coord(&gm);

    SpyPlot p1;
    SpyPlot p2;

    coord.registerPlot(QStringLiteral("p1"), &p1);
    coord.registerPlot(QStringLiteral("p2"), &p2);

    QSignalSpy cSpy(&coord, &GroupCoordinator::groupCreated);
    QSignalSpy mSpy(&coord, &GroupCoordinator::groupPropertiesChanged);
    QSignalSpy p1Spy(&p1, &SpyPlot::propsChanged);
    QSignalSpy p2Spy(&p2, &SpyPlot::propsChanged);

    int gid = gm.createGroup(QStringLiteral("G"));
    REQUIRE(gid >= 0);
    REQUIRE(cSpy.count() == 1);

    REQUIRE(gm.setGroupName(gid, QStringLiteral("G2")));
    REQUIRE(mSpy.count() == 1);
    REQUIRE(p1Spy.count() == 1);
    REQUIRE(p2Spy.count() == 1);
}

#include "GroupCoordinator.test.moc"

