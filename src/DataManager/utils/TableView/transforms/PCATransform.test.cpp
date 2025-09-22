#include "PCATransform.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/computers/LineSamplingMultiComputer.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PCATransform preserves EntityIds with IndexSelector rows sized to kept count",
          "[datamanager][tableview][pca][transform]") {
    // Build a minimal table with LineSamplingMultiComputer so we get numeric columns
    DataManager dm;

    // Prepare LineData with simple lines across timestamps 10,20,30 (2,2,1 entities)
    auto line_data = std::make_shared<LineData>();
    line_data->addAtTime(TimeFrameIndex(10), std::vector<Point2D<float>>{{0, 0}, {1, 1}});
    line_data->addAtTime(TimeFrameIndex(10), std::vector<Point2D<float>>{{2, 2}, {3, 3}});
    line_data->addAtTime(TimeFrameIndex(20), std::vector<Point2D<float>>{{4, 4}, {5, 5}});
    line_data->addAtTime(TimeFrameIndex(20), std::vector<Point2D<float>>{{6, 6}, {7, 7}});
    line_data->addAtTime(TimeFrameIndex(30), std::vector<Point2D<float>>{{8, 8}, {9, 9}});
    line_data->setImageSize({800, 600});

    // Identity context so entity IDs are generated
    line_data->setIdentityContext("test_lines", dm.getEntityRegistry());
    line_data->rebuildAllEntityIds();

    dm.setData<LineData>("test_lines", line_data, TimeKey("time"));

    // TimeFrame and row selector using timestamps [10,20,30]
    std::vector<int> timeValues = {10, 20, 30};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    auto adapter = std::make_shared<LineDataAdapter>(line_data, tf, "test_lines");

    // Build base table: entity-expanded to 5 rows
    auto dme = std::make_shared<DataManagerExtension>(dm);

    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};

    TableViewBuilder builder(dme);
    builder.setRowSelector(std::make_unique<TimestampSelector>(timestamps, tf));

    // Use 2 segments -> 3 sample points -> 6 columns
    auto lsmc = std::make_unique<LineSamplingMultiComputer>(
            std::static_pointer_cast<ILineSource>(adapter),
            "test_lines",
            tf,
            2);

    builder.addColumns<double>("Line", std::move(lsmc));
    auto base_table = builder.build();

    for (auto const & column_name : base_table.getColumnNames()) {
        base_table.getColumnValues<double>(column_name);
    }

    REQUIRE(base_table.getRowCount() == 5);// entity expansion: 2 + 2 + 1
    auto base_ids = base_table.getEntityIds();
    REQUIRE(base_ids.size() == 5);

    // Configure PCA to include all numeric columns, center only
    PCAConfig cfg;
    cfg.center = true;
    cfg.standardize = false;
    cfg.include = base_table.getColumnNames();

    PCATransform pca(cfg);
    auto transformed = pca.apply(base_table);


    auto transformed_ids = transformed.getEntityIds();
    REQUIRE(transformed_ids.size() == transformed.getRowCount());

    // The EntityIds should match the original table's first 5 rows
    for (size_t i = 0; i < transformed_ids.size(); ++i) {
        REQUIRE(transformed_ids[i] == base_ids[i]);
    }
}
