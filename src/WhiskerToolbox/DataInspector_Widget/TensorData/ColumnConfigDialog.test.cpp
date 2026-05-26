/**
 * @file ColumnConfigDialog.test.cpp
 * @brief Tests for ColumnConfigDialog pipeline library integration
 */

#include "TensorData/ColumnConfigDialog.hpp"
#include "TensorData/TensorDesigner.hpp"

#include "DataManager/DataManager.hpp"
#include "TransformsV2/core/PipelineLibrary.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QPushButton>

#include <memory>

using namespace WhiskerToolbox::Transforms::V2::Examples;

namespace {

class QtAppFixture {
public:
    QtAppFixture() {
        if (!QApplication::instance()) {
            int argc = 0;
            char * argv[] = {nullptr};
            _app = std::make_unique<QApplication>(argc, argv);
        }
    }

    std::unique_ptr<QApplication> _app;
};

}// namespace

TEST_CASE("ColumnConfigDialog exposes Load from Library when library dir is set",
          "[DataInspector][ColumnConfigDialog][library]") {

    QtAppFixture const qt;

    auto const config_dir =
            QDir::temp().filePath(QStringLiteral("wt_column_config_%1").arg(
                    QDateTime::currentMSecsSinceEpoch()));
    auto const dir_result = ensureUserPipelineDirectory(config_dir.toStdString());
    REQUIRE(dir_result);

    auto data_manager = std::make_shared<DataManager>();
    ColumnConfigDialog dialog(data_manager,
                              DesignerRowType::Ordinal,
                              nullptr,
                              QString::fromStdString(dir_result.value().string()));
    dialog.show();
    QApplication::processEvents();

    bool found = false;
    for (auto * button: dialog.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Load from Library...")) {
            found = true;
            CHECK(button->isEnabled());
        }
    }
    CHECK(found);
}

TEST_CASE("ColumnConfigDialog disables Load from Library without library dir",
          "[DataInspector][ColumnConfigDialog][library]") {

    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    ColumnConfigDialog dialog(data_manager, DesignerRowType::Ordinal);
    dialog.show();
    QApplication::processEvents();

    for (auto * button: dialog.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Load from Library...")) {
            CHECK_FALSE(button->isEnabled());
        }
    }
}
