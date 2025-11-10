// Catch2
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Qt
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QSpinBox>
#include <QTimer>

// Project
#include "Lines/Line_Loader_Widget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "IO/CapnProto/Line_Data_Binary.hpp"
#include "DataManager/IO/LoaderRegistration.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

class MessageBoxAutoCloser : public QObject {
public:
    explicit MessageBoxAutoCloser(QObject * parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject * watched, QEvent * event) override {
        if (event->type() == QEvent::Show) {
            if (auto * box = qobject_cast<QMessageBox *>(watched)) {
                QTimer::singleShot(0, box, &QMessageBox::accept);
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

// Helper to compare two LineData instances for equality (geometry + image size)
void verifyLineDataEquality(LineData const & original, LineData const & loaded) {
    auto original_times = original.getTimesWithData();
    auto loaded_times = loaded.getTimesWithData();

    std::vector<TimeFrameIndex> original_times_vec(original_times.begin(), original_times.end());
    std::vector<TimeFrameIndex> loaded_times_vec(loaded_times.begin(), loaded_times.end());

    REQUIRE(original_times_vec.size() == loaded_times_vec.size());

    std::sort(original_times_vec.begin(), original_times_vec.end());
    std::sort(loaded_times_vec.begin(), loaded_times_vec.end());

    for (size_t i = 0; i < original_times_vec.size(); ++i) {
        REQUIRE(original_times_vec[i] == loaded_times_vec[i]);

        TimeFrameIndex time = original_times_vec[i];
        auto const & original_lines = original.getAtTime(time);
        auto const & loaded_lines = loaded.getAtTime(time);

        REQUIRE(original_lines.size() == loaded_lines.size());

        for (size_t line_idx = 0; line_idx < original_lines.size(); ++line_idx) {
            Line2D const & orig_line = original_lines[line_idx];
            Line2D const & load_line = loaded_lines[line_idx];

            REQUIRE(orig_line.size() == load_line.size());

            for (size_t point_idx = 0; point_idx < orig_line.size(); ++point_idx) {
                Point2D<float> op = orig_line[point_idx];
                Point2D<float> lp = load_line[point_idx];
                REQUIRE_THAT(op.x, WithinAbs(lp.x, 0.001f));
                REQUIRE_THAT(op.y, WithinAbs(lp.y, 0.001f));
            }
        }
    }

    REQUIRE(original.getImageSize().width == loaded.getImageSize().width);
    REQUIRE(original.getImageSize().height == loaded.getImageSize().height);
}

// Create a small example LineData used in tests
std::shared_ptr<LineData> createExampleLineData() {
    auto line_data = std::make_shared<LineData>();

    std::vector<Point2D<float>> line1_points = {
            {10.0f, 20.0f},
            {30.0f, 40.0f},
            {50.0f, 60.0f}
    };
    Line2D line1(line1_points);

    std::vector<Point2D<float>> line2_points = {
            {100.0f, 100.0f},
            {150.0f, 100.0f},
            {150.0f, 150.0f}
    };
    Line2D line2(line2_points);

    std::vector<Point2D<float>> line3_points = {
            {200.0f, 200.0f},
            {250.0f, 250.0f},
            {300.0f, 200.0f},
            {350.0f, 250.0f}
    };
    Line2D line3(line3_points);

    line_data->addAtTime(TimeFrameIndex(0), line1, NotifyObservers::No);
    line_data->addAtTime(TimeFrameIndex(0), line2, NotifyObservers::No);
    line_data->addAtTime(TimeFrameIndex(1), line3, NotifyObservers::No);

    line_data->setImageSize(ImageSize(800, 600));
    return line_data;
}

} // namespace

TEST_CASE("UI - Line_Loader_Widget - Binary round trip using UI loader", "[LineLoaderWidget][Binary][UI][IO]") {
    // Ensure we have a QApplication for widget tests. Create once and intentionally
    // do not delete to avoid Qt teardown crashes under ASAN in headless mode.
    if (qApp == nullptr) {
        qputenv("QT_QPA_PLATFORM", QByteArray("minimal"));
        static int argc = 0;
        static char ** argv = nullptr;
        static QApplication * app = new QApplication(argc, argv);
        (void)app;
    }

    // Register available IO loaders (enables Binary loader when built with CapnProto)
    registerAllLoaders();

    // Auto-close any QMessageBox spawned by the widget
    static MessageBoxAutoCloser closer(qApp);
    qApp->installEventFilter(&closer);

    // Prepare test directory and binary file
    std::filesystem::path const test_dir = std::filesystem::current_path() / "test_binary_output_ui";
    std::filesystem::create_directories(test_dir);
    std::filesystem::path const binary_filepath = test_dir / "test_line_data_ui.capnp";

    // Create and save original LineData in binary format
    auto original = createExampleLineData();

    BinaryLineSaverOptions save_opts;
    save_opts.filename = binary_filepath.filename().string();
    save_opts.parent_dir = test_dir.string();
    REQUIRE(save(*original, save_opts));
    REQUIRE(std::filesystem::exists(binary_filepath));
    REQUIRE(std::filesystem::file_size(binary_filepath) > 0);

    // Create DataManager and widget
    auto dm = std::make_shared<DataManager>();
    Line_Loader_Widget widget(dm);

    // Configure widget controls: set target data name and scaling widget original size
    auto * name_edit = widget.findChild<QLineEdit *>("data_name_text");
    if (name_edit) {
        name_edit->setText("test_binary_lines_ui");
    }

    // Select Binary in loader type combo if present
    if (auto * combo = widget.findChild<QComboBox *>("loader_type_combo")) {
        int idx = combo->findText("Binary");
        if (idx >= 0) {
            combo->setCurrentIndex(idx);
        }
    }

    // Set original image size in scaling widget (so UI won't overwrite with zeros)
    if (auto * ow = widget.findChild<QSpinBox *>("original_width_spin")) {
        ow->setValue(800);
    }
    if (auto * oh = widget.findChild<QSpinBox *>("original_height_spin")) {
        oh->setValue(600);
    }

    // Schedule safety timer to close any message boxes that appear
    QTimer::singleShot(0, []() {
        for (QWidget * w : QApplication::topLevelWidgets()) {
            if (auto * box = qobject_cast<QMessageBox *>(w)) {
                box->accept();
            }
        }
    });

    // Invoke the private slot to load the binary file via UI path
    bool invoked = QMetaObject::invokeMethod(
            &widget,
            "_handleLoadBinaryFileRequested",
            Qt::DirectConnection,
            Q_ARG(QString, QString::fromStdString(binary_filepath.string())));
    REQUIRE(invoked);

    // Process events to let any async UI operations settle
    QApplication::processEvents();

    // Determine key used by widget
    std::string key = binary_filepath.stem().string();
    if (name_edit && !name_edit->text().isEmpty()) {
        key = name_edit->text().toStdString();
    }

    // Retrieve loaded data and compare
    auto loaded = dm->getData<LineData>(key);
    REQUIRE(loaded != nullptr);
    verifyLineDataEquality(*original, *loaded);

    // Cleanup
    std::error_code ec;
    std::filesystem::remove(binary_filepath, ec);
    std::filesystem::remove(test_dir, ec);
}

