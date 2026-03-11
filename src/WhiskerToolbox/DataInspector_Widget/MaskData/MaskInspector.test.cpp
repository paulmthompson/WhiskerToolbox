
#include "MaskInspector.hpp"
#include "MaskTableView.hpp"
#include "MaskTableModel.hpp"

#include "DataExport_Widget/Masks/CSV/CSVMaskSaver_Widget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "IO/formats/CSV/mask/Mask_Data_CSV.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
#include <QAbstractItemModel>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <numeric>
#include <vector>

namespace {
// Ensures a QApplication exists for the process. Must be called before using any Qt widgets/signals.
// Uses a leaked pointer intentionally - QApplication must outlive all Qt objects in tests.
void ensureQApplication()
{
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());// NOLINT: Intentionally leaked
    }
}

std::filesystem::path makeTempDir()
{
    auto dir = std::filesystem::temp_directory_path() / "whisker_mask_inspector_save_test";
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanupTempDir(std::filesystem::path const & dir)
{
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
}// namespace

// === MaskInspector Tests ===

TEST_CASE("MaskInspector construction", "[MaskInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        // Inspector should be created without crashing
        app->processEvents();
    }

    SECTION("Returns correct data type") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.getDataType() == DM_DataType::Mask);
        REQUIRE(inspector.getTypeName() == QStringLiteral("Mask"));
        REQUIRE(inspector.supportsExport() == true);
    }
}

TEST_CASE("MaskInspector has expected UI", "[MaskInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Contains main title label") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * title_label = inspector.findChild<QLabel *>("label_main_title");
        REQUIRE(title_label != nullptr);
        REQUIRE(title_label->text() == QStringLiteral("Mask Data Management"));

        app->processEvents();
    }

    SECTION("Contains image size status label") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * status_label = inspector.findChild<QLabel *>("image_size_status_label");
        REQUIRE(status_label != nullptr);
        REQUIRE(status_label->text() == QStringLiteral("Not Set"));

        app->processEvents();
    }

    SECTION("Contains image size input fields") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * width_edit = inspector.findChild<QLineEdit *>("image_width_edit");
        REQUIRE(width_edit != nullptr);
        REQUIRE(width_edit->placeholderText() == QStringLiteral("Width"));

        auto * height_edit = inspector.findChild<QLineEdit *>("image_height_edit");
        REQUIRE(height_edit != nullptr);
        REQUIRE(height_edit->placeholderText() == QStringLiteral("Height"));

        app->processEvents();
    }

    SECTION("Contains image size control buttons") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * apply_button = inspector.findChild<QPushButton *>("apply_image_size_button");
        REQUIRE(apply_button != nullptr);
        REQUIRE(apply_button->text() == QStringLiteral("Apply"));

        auto * copy_button = inspector.findChild<QPushButton *>("copy_image_size_button");
        REQUIRE(copy_button != nullptr);
        REQUIRE(copy_button->text() == QStringLiteral("Copy Size"));

        app->processEvents();
    }

    SECTION("Contains copy from media combo box") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * media_combo = inspector.findChild<QComboBox *>("copy_from_media_combo");
        REQUIRE(media_combo != nullptr);
        REQUIRE(media_combo->placeholderText() == QStringLiteral("Copy from media..."));

        app->processEvents();
    }

    SECTION("Contains SAM model button") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * sam_button = inspector.findChild<QPushButton *>("load_sam_button");
        REQUIRE(sam_button != nullptr);
        REQUIRE(sam_button->text() == QStringLiteral("Load SAM Model"));

        app->processEvents();
    }

    SECTION("Contains export section") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        // Export section is a Section widget (collapsible)
        auto * export_section = inspector.findChild<QWidget *>("export_section");
        REQUIRE(export_section != nullptr);

        app->processEvents();
    }

    SECTION("Contains export type combo box") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * export_type_combo = inspector.findChild<QComboBox *>("export_type_combo");
        REQUIRE(export_type_combo != nullptr);
        REQUIRE(export_type_combo->count() == 3);  // Should have HDF5, Image, and CSV (RLE)
        REQUIRE(export_type_combo->itemText(0) == QStringLiteral("HDF5"));
        REQUIRE(export_type_combo->itemText(1) == QStringLiteral("Image"));
        REQUIRE(export_type_combo->itemText(2) == QStringLiteral("CSV (RLE)"));

        app->processEvents();
    }

    SECTION("Contains stacked saver options widget") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * stacked_widget = inspector.findChild<QStackedWidget *>("stacked_saver_options");
        REQUIRE(stacked_widget != nullptr);
        REQUIRE(stacked_widget->count() == 3);  // Should have HDF5, Image, and CSV saver widgets

        app->processEvents();
    }

    SECTION("Contains export media frames checkbox") {
        auto data_manager = std::make_shared<DataManager>();
        MaskInspector inspector(data_manager, nullptr, nullptr);

        auto * media_frames_checkbox = inspector.findChild<QCheckBox *>("export_media_frames_checkbox");
        REQUIRE(media_frames_checkbox != nullptr);
        REQUIRE(media_frames_checkbox->text() == QStringLiteral("Export matching media frames"));
        REQUIRE(media_frames_checkbox->isChecked() == false);  // Should start unchecked

        app->processEvents();
    }
}

TEST_CASE("MaskInspector saves data from DataManager", "[MaskInspector][save]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Save button exports MaskData to CSV RLE") {
        auto temp_dir = makeTempDir();
        auto data_manager = std::make_shared<DataManager>();
        data_manager->setOutputPath(temp_dir.string());

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with image size and masks
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);
        mask_data->setImageSize(ImageSize(640, 480));

        Mask2D mask0 = {
            {100, 100},
            {101, 100},
            {102, 100},
            {100, 101},
            {101, 101},
            {102, 101}
        };
        Mask2D mask1 = {
            {50, 50},
            {51, 50},
            {50, 51}
        };
        mask_data->addAtTime(TimeFrameIndex(0), mask0, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10), mask1, NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        MaskInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_masks");

        app->processEvents();

        // Select CSV (RLE) export type and configure saver
        auto * export_type_combo = inspector.findChild<QComboBox *>("export_type_combo");
        REQUIRE(export_type_combo != nullptr);
        export_type_combo->setCurrentIndex(2);  // CSV (RLE)
        app->processEvents();

        auto * csv_saver = inspector.findChild<CSVMaskSaver_Widget *>("csv_mask_saver_widget");
        REQUIRE(csv_saver != nullptr);

        auto * directory_edit = csv_saver->findChild<QLineEdit *>("directory_path_edit");
        REQUIRE(directory_edit != nullptr);
        directory_edit->setText(QString::fromStdString(temp_dir.string()));

        auto * filename_edit = csv_saver->findChild<QLineEdit *>("filename_edit");
        REQUIRE(filename_edit != nullptr);
        filename_edit->setText(QStringLiteral("saved_masks.csv"));

        // Schedule dialog dismissal before clicking save (QMessageBox blocks)
        QTimer::singleShot(50, []() {
            if (auto * w = QApplication::activeModalWidget()) {
                w->close();
            }
        });

        auto * save_button = csv_saver->findChild<QPushButton *>("save_button");
        REQUIRE(save_button != nullptr);
        save_button->click();

        app->processEvents();

        auto const filepath = temp_dir / "saved_masks.csv";
        REQUIRE(std::filesystem::exists(filepath));

        // Verify file content: load back and check mask data
        CSVMaskRLELoaderOptions load_opts;
        load_opts.filepath = filepath.string();

        auto loaded = load(load_opts);
        REQUIRE(loaded.size() == 2);  // Frames 0 and 10
        REQUIRE(loaded.count(TimeFrameIndex(0)) == 1);
        REQUIRE(loaded.count(TimeFrameIndex(10)) == 1);
        REQUIRE(loaded.at(TimeFrameIndex(0)).size() == 1);
        REQUIRE(loaded.at(TimeFrameIndex(10)).size() == 1);

        auto const & loaded_mask0 = loaded.at(TimeFrameIndex(0))[0];
        auto const & loaded_mask1 = loaded.at(TimeFrameIndex(10))[0];

        REQUIRE(loaded_mask0.size() == mask0.size());
        REQUIRE(loaded_mask1.size() == mask1.size());

        // Verify mask0 pixels (order may differ after RLE round-trip)
        std::vector<Point2D<uint32_t>> mask0_vec(mask0.begin(), mask0.end());
        std::vector<Point2D<uint32_t>> loaded0_vec(loaded_mask0.begin(), loaded_mask0.end());
        std::sort(mask0_vec.begin(), mask0_vec.end(), [](auto const & a, auto const & b) {
            return a.x != b.x ? a.x < b.x : a.y < b.y;
        });
        std::sort(loaded0_vec.begin(), loaded0_vec.end(), [](auto const & a, auto const & b) {
            return a.x != b.x ? a.x < b.x : a.y < b.y;
        });
        REQUIRE(mask0_vec == loaded0_vec);

        cleanupTempDir(temp_dir);
    }
}

TEST_CASE("MaskInspector image size display updates", "[MaskInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Updates image size display when active key is set") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with image size
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);
        mask_data->setImageSize(ImageSize(640, 480));

        // Add a simple mask
        Mask2D mask = {
            {100, 100},
            {101, 100},
            {102, 100},
            {100, 101},
            {101, 101},
            {102, 101}
        };
        mask_data->addAtTime(TimeFrameIndex(0), mask, NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        // Create inspector
        MaskInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_masks");

        app->processEvents();

        // Check that image size status label was updated
        auto * status_label = inspector.findChild<QLabel *>("image_size_status_label");
        REQUIRE(status_label != nullptr);
        REQUIRE(status_label->text() == QStringLiteral("640 × 480"));

        // Check that input fields were populated
        auto * width_edit = inspector.findChild<QLineEdit *>("image_width_edit");
        REQUIRE(width_edit != nullptr);
        REQUIRE(width_edit->text() == QStringLiteral("640"));

        auto * height_edit = inspector.findChild<QLineEdit *>("image_height_edit");
        REQUIRE(height_edit != nullptr);
        REQUIRE(height_edit->text() == QStringLiteral("480"));
    }

    SECTION("Shows 'Not Set' when image size is not set") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData without image size
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);
        // Don't set image size

        // Add a simple mask
        Mask2D mask = {
            {100, 100},
            {101, 100}
        };
        mask_data->addAtTime(TimeFrameIndex(0), mask, NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        // Create inspector
        MaskInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_masks");

        app->processEvents();

        // Check that image size status label shows "Not Set"
        auto * status_label = inspector.findChild<QLabel *>("image_size_status_label");
        REQUIRE(status_label != nullptr);
        REQUIRE(status_label->text() == QStringLiteral("Not Set"));

        // Check that input fields are empty
        auto * width_edit = inspector.findChild<QLineEdit *>("image_width_edit");
        REQUIRE(width_edit != nullptr);
        REQUIRE(width_edit->text().isEmpty());

        auto * height_edit = inspector.findChild<QLineEdit *>("image_height_edit");
        REQUIRE(height_edit != nullptr);
        REQUIRE(height_edit->text().isEmpty());
    }
}

TEST_CASE("MaskInspector group filter updates when groups are added", "[MaskInspector][Groups]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Group filter combo updates when groups are created") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        MaskInspector inspector(data_manager, group_manager.get(), nullptr);
        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        // Initially should have only "All Groups"
        REQUIRE(group_filter_combo->count() == 1);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));

        // Create first group
        int group1_id = group_manager->createGroup("Group A");
        app->processEvents();

        // Combo box should now have "All Groups" and "Group A"
        REQUIRE(group_filter_combo->count() == 2);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));

        // Create second group
        int group2_id = group_manager->createGroup("Group B");
        app->processEvents();

        // Combo box should now have "All Groups", "Group A", and "Group B"
        REQUIRE(group_filter_combo->count() == 3);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group B"));

        // Create third group
        int group3_id = group_manager->createGroup("Group C");
        app->processEvents();

        // Combo box should now have all four items
        REQUIRE(group_filter_combo->count() == 4);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group B"));
        REQUIRE(group_filter_combo->itemText(3) == QStringLiteral("Group C"));
    }

    SECTION("Group filter combo updates when groups are removed") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        MaskInspector inspector(data_manager, group_manager.get(), nullptr);
        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        // Create groups
        int group1_id = group_manager->createGroup("Group A");
        int group2_id = group_manager->createGroup("Group B");
        int group3_id = group_manager->createGroup("Group C");
        app->processEvents();

        REQUIRE(group_filter_combo->count() == 4);  // "All Groups" + 3 groups

        // Remove middle group
        group_manager->removeGroup(group2_id);
        app->processEvents();

        // Should still have "All Groups" + 2 groups
        REQUIRE(group_filter_combo->count() == 3);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group C"));
    }
}

// === MaskTableView Tests ===

TEST_CASE("MaskTableView construction", "[MaskTableView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        MaskTableView view(data_manager, nullptr);

        // View should be created without crashing
        app->processEvents();
    }

    SECTION("Returns correct data type") {
        auto data_manager = std::make_shared<DataManager>();
        MaskTableView view(data_manager, nullptr);

        REQUIRE(view.getDataType() == DM_DataType::Mask);
        REQUIRE(view.getTypeName() == QStringLiteral("Mask Table"));
    }

    SECTION("Has table view") {
        auto data_manager = std::make_shared<DataManager>();
        MaskTableView view(data_manager, nullptr);

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        REQUIRE(table_view->model() != nullptr);
    }
}

TEST_CASE("MaskTableView displays mask data", "[MaskTableView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Table shows masks from MaskData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with some masks
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add masks at different frames
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10), create_mask(30, 30), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(20), create_mask(40, 40), NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        MaskTableView view(data_manager, nullptr);
        view.setActiveKey("test_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Should have 4 rows (4 masks)
        REQUIRE(model->rowCount() == 4);

        // Verify frame column data
        REQUIRE(model->data(model->index(0, 0)).toInt() == 0);
        REQUIRE(model->data(model->index(1, 0)).toInt() == 0);
        REQUIRE(model->data(model->index(2, 0)).toInt() == 10);
        REQUIRE(model->data(model->index(3, 0)).toInt() == 20);
    }
}

TEST_CASE("MaskTableView updates automatically when masks are added", "[MaskTableView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Table updates when new masks are added to MaskData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with initial masks
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add initial masks (without notifying observers)
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10), create_mask(20, 20), NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        MaskTableView view(data_manager, nullptr);
        view.setActiveKey("test_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should have 2 rows
        REQUIRE(model->rowCount() == 2);

        // Add a new mask with NotifyObservers::Yes (should trigger automatic update)
        mask_data->addAtTime(TimeFrameIndex(20), create_mask(30, 30), NotifyObservers::Yes);
        app->processEvents();

        // Table should now have 3 rows (automatically updated)
        REQUIRE(model->rowCount() == 3);
        REQUIRE(model->data(model->index(2, 0)).toInt() == 20);

        // Add another mask
        mask_data->addAtTime(TimeFrameIndex(30), create_mask(40, 40), NotifyObservers::Yes);
        app->processEvents();

        // Table should now have 4 rows
        REQUIRE(model->rowCount() == 4);
        REQUIRE(model->data(model->index(3, 0)).toInt() == 30);

        // Add a mask at an existing frame
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(50, 50), NotifyObservers::Yes);
        app->processEvents();

        // Table should now have 5 rows (new mask added at frame 0)
        REQUIRE(model->rowCount() == 5);
    }

    SECTION("Table updates when masks are added at existing frames") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            return mask;
        };

        // Add initial mask at frame 0
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        MaskTableView view(data_manager, nullptr);
        view.setActiveKey("test_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should have 1 row
        REQUIRE(model->rowCount() == 1);
        REQUIRE(model->data(model->index(0, 0)).toInt() == 0);

        // Add another mask at the same frame (should create a new row)
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::Yes);
        app->processEvents();

        // Table should now have 2 rows (both at frame 0)
        REQUIRE(model->rowCount() == 2);
        REQUIRE(model->data(model->index(0, 0)).toInt() == 0);
        REQUIRE(model->data(model->index(1, 0)).toInt() == 0);
    }
}

TEST_CASE("MaskTableView group filtering", "[MaskTableView][Groups]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Table filters by group correctly when combo box selection changes") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with masks
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add masks at different frames
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);  // Will be Group A
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::No);  // Will be Group B
        mask_data->addAtTime(TimeFrameIndex(10), create_mask(30, 30), NotifyObservers::No); // Will be Group A
        mask_data->addAtTime(TimeFrameIndex(20), create_mask(40, 40), NotifyObservers::No); // Will be ungrouped

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        // Get entity IDs for the masks
        auto entity_ids_frame0_view = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10_view = mask_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> entity_ids_frame0(entity_ids_frame0_view.begin(), entity_ids_frame0_view.end());
        std::vector<EntityId> entity_ids_frame10(entity_ids_frame10_view.begin(), entity_ids_frame10_view.end());
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];  // First mask at frame 0
        EntityId entity1 = entity_ids_frame0[1];   // Second mask at frame 0
        EntityId entity2 = entity_ids_frame10[0];  // Mask at frame 10

        // Create inspector and view, and connect them
        MaskInspector inspector(data_manager, group_manager.get(), nullptr);
        MaskTableView view(data_manager, nullptr);
        inspector.setDataView(&view);

        inspector.setActiveKey("test_masks");
        view.setActiveKey("test_masks");

        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should show all masks and have only "All Groups" in combo
        REQUIRE(model->rowCount() == 4);
        REQUIRE(group_filter_combo->count() == 1);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));

        // Create groups
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");
        app->processEvents();

        // Combo box should update
        REQUIRE(group_filter_combo->count() == 3);  // "All Groups" + 2 groups
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group B"));

        // Assign entities to groups
        group_manager->assignEntitiesToGroup(group_a_id, {entity0, entity2});
        group_manager->assignEntitiesToGroup(group_b_id, {entity1});
        app->processEvents();

        // Table should still show all masks (no filter applied yet)
        REQUIRE(model->rowCount() == 4);

        // Filter by Group A using the combo box (this should trigger _onGroupFilterChanged)
        group_filter_combo->setCurrentIndex(1);
        app->processEvents();

        // Table should now show only 2 masks (entity0 and entity2)
        REQUIRE(model->rowCount() == 2);

        // Change filter to Group B using the combo box
        group_filter_combo->setCurrentIndex(2);
        app->processEvents();

        // Table should now show only 1 mask (entity1)
        REQUIRE(model->rowCount() == 1);

        // Clear filter by selecting "All Groups" in the combo box
        group_filter_combo->setCurrentIndex(0);
        app->processEvents();

        // Table should show all 4 masks again
        REQUIRE(model->rowCount() == 4);
    }
}

TEST_CASE("MaskInspector and MaskTableView move and copy operations", "[MaskInspector][MaskTableView][MoveCopy]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Move masks to target MaskData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create source MaskData with masks
        auto source_mask_data = std::make_shared<MaskData>();
        source_mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add masks to source
        source_mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);
        source_mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::No);
        source_mask_data->addAtTime(TimeFrameIndex(10), create_mask(30, 30), NotifyObservers::No);
        source_mask_data->addAtTime(TimeFrameIndex(20), create_mask(40, 40), NotifyObservers::No);

        data_manager->setData<MaskData>("source_masks", source_mask_data, TimeKey("time"));

        // Create target MaskData (empty)
        auto target_mask_data = std::make_shared<MaskData>();
        target_mask_data->setTimeFrame(tf);
        data_manager->setData<MaskData>("target_masks", target_mask_data, TimeKey("time"));

        // Get entity IDs from source
        auto entity_ids_frame0_view = source_mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10_view = source_mask_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> entity_ids_frame0(entity_ids_frame0_view.begin(), entity_ids_frame0_view.end());
        std::vector<EntityId> entity_ids_frame10(entity_ids_frame10_view.begin(), entity_ids_frame10_view.end());
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];

        // Create inspector and view, and connect them
        MaskInspector inspector(data_manager, nullptr, nullptr);
        MaskTableView view(data_manager, nullptr);
        inspector.setDataView(&view);

        inspector.setActiveKey("source_masks");
        view.setActiveKey("source_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially source should have 4 masks, target should have 0
        REQUIRE(model->rowCount() == 4);
        REQUIRE(target_mask_data->getTimesWithData().size() == 0);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);
        REQUIRE(std::find(selected_entity_ids.begin(), selected_entity_ids.end(), entity0) != selected_entity_ids.end());
        REQUIRE(std::find(selected_entity_ids.begin(), selected_entity_ids.end(), entity1) != selected_entity_ids.end());

        // Emit move signal (simulating context menu selection)
        emit view.moveMasksRequested("target_masks");
        app->processEvents();

        // Source should now have 2 masks (entity2 and the one at frame 20)
        view.updateView();
        app->processEvents();
        REQUIRE(model->rowCount() == 2);

        // Target should have 2 masks (entity0 and entity1)
        auto target_times = target_mask_data->getTimesWithData();
        REQUIRE(target_times.size() == 1);  // Should have data at frame 0
        REQUIRE(target_mask_data->getAtTime(TimeFrameIndex(0)).size() == 2);

        // Verify source still has entity2
        auto source_entity_ids_frame10_view = source_mask_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> source_entity_ids_frame10(source_entity_ids_frame10_view.begin(), source_entity_ids_frame10_view.end());
        REQUIRE(source_entity_ids_frame10.size() == 1);
        REQUIRE(source_entity_ids_frame10[0] == entity2);
    }

    SECTION("Copy masks to target MaskData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create source MaskData with masks
        auto source_mask_data = std::make_shared<MaskData>();
        source_mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add masks to source
        source_mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);
        source_mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::No);
        source_mask_data->addAtTime(TimeFrameIndex(10), create_mask(30, 30), NotifyObservers::No);

        data_manager->setData<MaskData>("source_masks", source_mask_data, TimeKey("time"));

        // Create target MaskData (empty)
        auto target_mask_data = std::make_shared<MaskData>();
        target_mask_data->setTimeFrame(tf);
        data_manager->setData<MaskData>("target_masks", target_mask_data, TimeKey("time"));

        // Get entity IDs from source
        auto entity_ids_frame0_view = source_mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
        std::vector<EntityId> entity_ids_frame0(entity_ids_frame0_view.begin(), entity_ids_frame0_view.end());
        REQUIRE(entity_ids_frame0.size() == 2);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];

        // Create inspector and view, and connect them
        MaskInspector inspector(data_manager, nullptr, nullptr);
        MaskTableView view(data_manager, nullptr);
        inspector.setDataView(&view);

        inspector.setActiveKey("source_masks");
        view.setActiveKey("source_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially source should have 3 masks, target should have 0
        REQUIRE(model->rowCount() == 3);
        REQUIRE(target_mask_data->getTimesWithData().size() == 0);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);

        // Emit copy signal (simulating context menu selection)
        emit view.copyMasksRequested("target_masks");
        app->processEvents();

        // Source should still have 3 masks (unchanged)
        view.updateView();
        app->processEvents();
        REQUIRE(model->rowCount() == 3);

        // Target should have 2 masks (copies of entity0 and entity1)
        auto target_times = target_mask_data->getTimesWithData();
        REQUIRE(target_times.size() == 1);  // Should have data at frame 0
        REQUIRE(target_mask_data->getAtTime(TimeFrameIndex(0)).size() == 2);

        // Verify source still has all original masks
        REQUIRE(source_mask_data->getAtTime(TimeFrameIndex(0)).size() == 2);
        REQUIRE(source_mask_data->getAtTime(TimeFrameIndex(10)).size() == 1);
    }
}

// === Group Management Context Menu Tests ===

TEST_CASE("MaskInspector and MaskTableView group management context menu", "[MaskInspector][MaskTableView][GroupManagement]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Move masks to group via context menu") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with masks
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add masks
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10), create_mask(30, 30), NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0_view = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10_view = mask_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> entity_ids_frame0(entity_ids_frame0_view.begin(), entity_ids_frame0_view.end());
        std::vector<EntityId> entity_ids_frame10(entity_ids_frame10_view.begin(), entity_ids_frame10_view.end());
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];

        // Create groups
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");
        app->processEvents();

        // Create inspector and view, and connect them
        MaskInspector inspector(data_manager, group_manager.get(), nullptr);
        MaskTableView view(data_manager, nullptr);
        inspector.setDataView(&view);

        inspector.setActiveKey("test_masks");
        view.setActiveKey("test_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially, no entities should be in groups
        REQUIRE(group_manager->getEntityGroup(entity0) == -1);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);
        REQUIRE(group_manager->getEntityGroup(entity2) == -1);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);

        // Emit move to group signal (simulating context menu selection)
        emit view.moveMasksToGroupRequested(group_a_id);
        app->processEvents();

        // Verify entities are now in Group A
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity2) == -1);  // Not selected, should remain ungrouped

        // Verify table updates to show group names
        view.updateView();
        app->processEvents();
        
        // Check that the group names are updated in the model
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<MaskTableModel *>(model)->getRowData(row);
            if (row_data.entity_id == entity0 || row_data.entity_id == entity1) {
                REQUIRE(row_data.group_name == QStringLiteral("Group A"));
            } else if (row_data.entity_id == entity2) {
                REQUIRE(row_data.group_name == QStringLiteral("No Group"));
            }
        }

        // Now select entity2 and move it to Group B
        selection_model->clearSelection();
        selection_model->select(model->index(2, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        emit view.moveMasksToGroupRequested(group_b_id);
        app->processEvents();

        // Verify entity2 is now in Group B
        REQUIRE(group_manager->getEntityGroup(entity2) == group_b_id);
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);  // Should still be in Group A
        REQUIRE(group_manager->getEntityGroup(entity1) == group_a_id);  // Should still be in Group A
    }

    SECTION("Remove masks from group via context menu") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with masks
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add masks
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10), create_mask(30, 30), NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0_view = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10_view = mask_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> entity_ids_frame0(entity_ids_frame0_view.begin(), entity_ids_frame0_view.end());
        std::vector<EntityId> entity_ids_frame10(entity_ids_frame10_view.begin(), entity_ids_frame10_view.end());
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];

        // Create group and assign entities
        int group_a_id = group_manager->createGroup("Group A");
        group_manager->assignEntitiesToGroup(group_a_id, {entity0, entity1, entity2});
        app->processEvents();

        // Verify all entities are in Group A
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity2) == group_a_id);

        // Create inspector and view, and connect them
        MaskInspector inspector(data_manager, group_manager.get(), nullptr);
        MaskTableView view(data_manager, nullptr);
        inspector.setDataView(&view);

        inspector.setActiveKey("test_masks");
        view.setActiveKey("test_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);

        // Emit remove from group signal (simulating context menu selection)
        emit view.removeMasksFromGroupRequested();
        app->processEvents();

        // Verify selected entities are removed from group
        REQUIRE(group_manager->getEntityGroup(entity0) == -1);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);
        REQUIRE(group_manager->getEntityGroup(entity2) == group_a_id);  // Not selected, should remain in group

        // Verify table updates to show group names
        view.updateView();
        app->processEvents();
        
        // Check that the group names are updated in the model
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<MaskTableModel *>(model)->getRowData(row);
            if (row_data.entity_id == entity0 || row_data.entity_id == entity1) {
                REQUIRE(row_data.group_name == QStringLiteral("No Group"));
            } else if (row_data.entity_id == entity2) {
                REQUIRE(row_data.group_name == QStringLiteral("Group A"));
            }
        }
    }

    SECTION("Move masks from one group to another via context menu") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create MaskData with masks
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(tf);

        // Helper to create a simple mask
        auto create_mask = [](uint32_t base_x, uint32_t base_y) -> Mask2D {
            Mask2D mask;
            mask.push_back(Point2D<uint32_t>{base_x, base_y});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y});
            mask.push_back(Point2D<uint32_t>{base_x, base_y + 1});
            mask.push_back(Point2D<uint32_t>{base_x + 1, base_y + 1});
            return mask;
        };

        // Add masks
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(10, 10), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(20, 20), NotifyObservers::No);

        data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0_view = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
        std::vector<EntityId> entity_ids_frame0(entity_ids_frame0_view.begin(), entity_ids_frame0_view.end());
        REQUIRE(entity_ids_frame0.size() == 2);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];

        // Create groups and assign entity0 to Group A
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");
        group_manager->assignEntitiesToGroup(group_a_id, {entity0});
        app->processEvents();

        // Verify initial group assignment
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);

        // Create inspector and view, and connect them
        MaskInspector inspector(data_manager, group_manager.get(), nullptr);
        MaskTableView view(data_manager, nullptr);
        inspector.setDataView(&view);

        inspector.setActiveKey("test_masks");
        view.setActiveKey("test_masks");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Select first row (entity0)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Move entity0 from Group A to Group B
        emit view.moveMasksToGroupRequested(group_b_id);
        app->processEvents();

        // Verify entity0 is now in Group B (moved from Group A)
        REQUIRE(group_manager->getEntityGroup(entity0) == group_b_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);  // Should remain ungrouped

        // Verify table updates
        view.updateView();
        app->processEvents();
        
        // Check that the group name is updated in the model
        auto row_data = static_cast<MaskTableModel *>(model)->getRowData(0);
        REQUIRE(row_data.entity_id == entity0);
        REQUIRE(row_data.group_name == QStringLiteral("Group B"));
    }
}
