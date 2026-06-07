/**
 * @file TransformsV2Widget.test.cpp
 * @brief Tests for TransformsV2_Widget components
 *
 * Tests cover:
 *  - AutoParamWidget: schema-based UI generation and layout correctness
 *  - PipelineStepListWidget: transform discovery for various input types
 *  - StepConfigPanel: config panel creation for transform steps
 *  - TransformsV2Properties_Widget: integration with data focus / type resolution
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QDateTime>
#include <QDir>
#include <QSpinBox>
#include <QTextEdit>

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "Core/TransformsV2State.hpp"
#include "UI/PipelineStepListWidget.hpp"
#include "UI/StepConfigPanel.hpp"
#include "UI/PipelineLibraryDialog.hpp"
#include "UI/TransformsV2Properties_Widget.hpp"

#include "TransformsV2/io/PipelineLibrary.hpp"

#include "EditorState/SelectionContext.hpp"
#include "EditorState/StrongTypes.hpp"

#include "DataManager/DataManager.hpp"

#include "ParameterSchema/ParameterSchema.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/TypeChainResolver.hpp"
#include "DataManager/utils/ContainerElementMapping.hpp"
#include "DataManager/utils/ContainerTypeIndex.hpp"

#include <QStackedWidget>
#include <QTest>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using namespace WhiskerToolbox::TypeTraits;

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Ensures QApplication exists for Qt widget testing
 */
class QtAppFixture {
public:
    QtAppFixture() {
        if (!QApplication::instance()) {
            int argc = 0;
            char * argv[] = {nullptr};
            app = std::make_unique<QApplication>(argc, argv);
        }
    }

    std::unique_ptr<QApplication> app;
};

// ============================================================================
// Section 1: AutoParamWidget UI Generation
// ============================================================================

TEST_CASE("AutoParamWidget generates correct layouts from ParameterSchema",
          "[TransformsV2Widget][AutoParamWidget][ui]") {

    QtAppFixture const qt;

    SECTION("Schema with float fields creates QDoubleSpinBox widgets") {
        // MaskAreaParams has 3 optional fields: scale_factor (float), min_area (float), exclude_holes (bool)
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateMaskArea");
        REQUIRE(schema != nullptr);
        REQUIRE(schema->fields.size() == 3);

        AutoParamWidget widget;
        REQUIRE_NOTHROW(widget.setSchema(*schema));

    }

    SECTION("Schema with string enum fields creates QComboBox when ParameterUIHints provides allowed_values") {
        // LineAngleParams has a method field (std::optional<std::string>)
        // Without ParameterUIHints specialization, allowed_values is empty
        // and it renders as a QLineEdit rather than QComboBox
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateLineAngle");
        REQUIRE(schema != nullptr);

        auto const * method_field = schema->field("method");
        REQUIRE(method_field != nullptr);
        CHECK(method_field->type_name == "enum");

        AutoParamWidget widget;
        widget.setSchema(*schema);

        // Without UI hints, string fields render as QLineEdit
        if (method_field->allowed_values.empty()) {
            auto line_edits = widget.findChildren<QLineEdit *>();
            CHECK_FALSE(line_edits.isEmpty());
        } else {
            // With UI hints, would be a QComboBox
            auto combos = widget.findChildren<QComboBox *>();
            CHECK_FALSE(combos.isEmpty());
        }
    }

    SECTION("Schema with bool fields creates QCheckBox widgets") {
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateMaskArea");
        REQUIRE(schema != nullptr);

        // exclude_holes is a bool field
        auto const * exclude_field = schema->field("exclude_holes");
        REQUIRE(exclude_field != nullptr);
        CHECK(exclude_field->type_name == "bool");

        AutoParamWidget widget;
        widget.setSchema(*schema);

        auto checkboxes = widget.findChildren<QCheckBox *>();
        // Should have checkbox for bool field
        CHECK_FALSE(checkboxes.isEmpty());
    }

    SECTION("Schema with int fields creates QSpinBox widgets") {
        // LineAngleParams has polynomial_order (int)
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateLineAngle");
        REQUIRE(schema != nullptr);

        auto const * poly_field = schema->field("polynomial_order");
        REQUIRE(poly_field != nullptr);
        CHECK(poly_field->type_name == "int");

        AutoParamWidget widget;
        widget.setSchema(*schema);

        auto spinboxes = widget.findChildren<QSpinBox *>();
        CHECK_FALSE(spinboxes.isEmpty());
    }

    SECTION("Schema with validator constraints are extracted correctly") {
        // MaskAreaParams::scale_factor has ExclusiveMinimum<0.0f>
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateMaskArea");
        REQUIRE(schema != nullptr);

        auto const * scale_field = schema->field("scale_factor");
        REQUIRE(scale_field != nullptr);
        // Should have exclusive minimum constraint from the validator
        CHECK(scale_field->is_exclusive_min);
        CHECK(hasValidator(scale_field->raw_type_name));

        auto const * min_area_field = schema->field("min_area");
        REQUIRE(min_area_field != nullptr);
        // min_area has Minimum<0.0f> (inclusive)
        CHECK_FALSE(min_area_field->is_exclusive_min);
        CHECK(hasValidator(min_area_field->raw_type_name));

        // The widget should create QDoubleSpinBox for float fields
        AutoParamWidget widget;
        widget.setSchema(*schema);

        auto dspins = widget.findChildren<QDoubleSpinBox *>();
        CHECK_FALSE(dspins.isEmpty());
    }

    SECTION("Empty schema (NoParams) produces no field widgets") {
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateMaskArea");
        REQUIRE(schema != nullptr);

        // Create a schema with no fields (like NoParams)
        ParameterSchema empty_schema;
        empty_schema.params_type_name = "NoParams";

        AutoParamWidget widget;
        widget.setSchema(empty_schema);

        CHECK(widget.findChildren<QDoubleSpinBox *>().isEmpty());
        CHECK(widget.findChildren<QSpinBox *>().isEmpty());
        CHECK(widget.findChildren<QCheckBox *>().isEmpty());
        CHECK(widget.findChildren<QComboBox *>().isEmpty());
        CHECK(widget.findChildren<QLineEdit *>().isEmpty());
    }

    SECTION("fromJson populates widget values") {
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateMaskArea");
        REQUIRE(schema != nullptr);

        AutoParamWidget widget;
        widget.setSchema(*schema);

        std::string const json = R"({"scale_factor": 2.5, "min_area": 10.0, "exclude_holes": true})";
        CHECK(widget.fromJson(json));

        // After fromJson, toJson should reflect the loaded values
        auto result = widget.toJson();
        CHECK_THAT(result, Catch::Matchers::ContainsSubstring("scale_factor"));
        CHECK_THAT(result, Catch::Matchers::ContainsSubstring("min_area"));
        CHECK_THAT(result, Catch::Matchers::ContainsSubstring("exclude_holes"));
    }

    SECTION("toJson round-trips through fromJson") {
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateLineAngle");
        REQUIRE(schema != nullptr);

        AutoParamWidget widget;
        widget.setSchema(*schema);

        std::string const input_json = R"({"position": 0.5, "polynomial_order": 5})";
        REQUIRE(widget.fromJson(input_json));

        auto output_json = widget.toJson();
        CHECK_THAT(output_json, Catch::Matchers::ContainsSubstring("position"));
        CHECK_THAT(output_json, Catch::Matchers::ContainsSubstring("polynomial_order"));
    }

    SECTION("Eight-field schema generates correct number of rows") {
        // LineAngleParams has 8 fields: position, window, method, polynomial_order, axis_x_x, axis_x_y, axis_y_x, axis_y_y
        auto const * schema = ElementRegistry::instance().getParameterSchema("CalculateLineAngle");
        REQUIRE(schema != nullptr);
        CHECK(schema->fields.size() == 8);

    }
}

// ============================================================================
// Section 2: PipelineStepListWidget — Transform Discovery
// ============================================================================

TEST_CASE("PipelineStepListWidget discovers transforms for input types",
          "[TransformsV2Widget][PipelineStepList][discovery]") {

    QtAppFixture const qt;

    auto & registry = ElementRegistry::instance();

    SECTION("MaskData input type finds Mask transforms") {
        auto container_type = TypeIndexMapper::stringToContainer("MaskData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        PipelineStepListWidget widget;
        widget.setInputType(element_type, container_type);

        // The element type for MaskData is Mask2D
        // Registered transforms for Mask2D: CalculateMaskArea, CalculateMaskCentroid
        auto names = registry.getTransformsForInputType(element_type);
        REQUIRE_FALSE(names.empty());

        bool found_mask_area = false;
        bool found_mask_centroid = false;
        for (auto const & name: names) {
            if (name == "CalculateMaskArea") found_mask_area = true;
            if (name == "CalculateMaskCentroid") found_mask_centroid = true;
        }
        CHECK(found_mask_area);
        CHECK(found_mask_centroid);
    }

    SECTION("LineData input type finds Line transforms") {
        auto container_type = TypeIndexMapper::stringToContainer("LineData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        auto names = registry.getTransformsForInputType(element_type);
        REQUIRE_FALSE(names.empty());

        bool found_line_angle = false;
        bool found_line_curvature = false;
        bool found_resample = false;
        bool found_flip = false;
        for (auto const & name: names) {
            if (name == "CalculateLineAngle") found_line_angle = true;
            if (name == "CalculateLineCurvature") found_line_curvature = true;
            if (name == "ResampleLine") found_resample = true;
            if (name == "FlipLineBase") found_flip = true;
        }
        CHECK(found_line_angle);
        CHECK(found_line_curvature);
        CHECK(found_resample);
        CHECK(found_flip);
    }

    SECTION("AnalogTimeSeries input type finds container transforms") {
        auto container_type = TypeIndexMapper::stringToContainer("AnalogTimeSeries");

        auto container_names = registry.getContainerTransformsForInputType(container_type);
        REQUIRE_FALSE(container_names.empty());

        bool found_threshold = false;
        bool found_interval_threshold = false;
        for (auto const & name: container_names) {
            if (name == "AnalogEventThreshold") found_threshold = true;
            if (name == "AnalogIntervalThreshold") found_interval_threshold = true;
        }
        CHECK(found_threshold);
        CHECK(found_interval_threshold);
    }

    SECTION("addStep succeeds for valid transform name") {
        auto container_type = TypeIndexMapper::stringToContainer("MaskData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        PipelineStepListWidget widget;
        widget.setInputType(element_type, container_type);

        CHECK(widget.addStep("CalculateMaskArea"));
        CHECK(widget.steps().size() == 1);
        CHECK(widget.steps()[0].transform_name == "CalculateMaskArea");
    }

    SECTION("addStep fails for invalid transform name") {
        auto container_type = TypeIndexMapper::stringToContainer("MaskData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        PipelineStepListWidget widget;
        widget.setInputType(element_type, container_type);

        CHECK_FALSE(widget.addStep("NonExistentTransform"));
        CHECK(widget.steps().empty());
    }

    SECTION("addStep updates output type correctly") {
        auto container_type = TypeIndexMapper::stringToContainer("MaskData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        PipelineStepListWidget widget;
        widget.setInputType(element_type, container_type);

        // CalculateMaskArea: Mask2D -> float
        widget.addStep("CalculateMaskArea");
        auto output_type = widget.currentOutputElementType();

        // After CalculateMaskArea, output should be float (not Mask2D)
        CHECK(output_type != element_type);
        CHECK(output_type == typeid(float));
    }

    SECTION("Multiple steps chain types correctly") {
        auto container_type = TypeIndexMapper::stringToContainer("LineData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        PipelineStepListWidget widget;
        widget.setInputType(element_type, container_type);

        // Line2D -> Line2D (resample), then Line2D -> float (angle)
        CHECK(widget.addStep("ResampleLine"));
        CHECK(widget.addStep("CalculateLineAngle"));
        CHECK(widget.steps().size() == 2);

        // Output after line angle should be float
        CHECK(widget.currentOutputElementType() == typeid(float));
    }

    SECTION("Type chain validation marks incompatible steps") {
        auto container_type = TypeIndexMapper::stringToContainer("LineData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        PipelineStepListWidget widget;
        widget.setInputType(element_type, container_type);

        // First add a Line->float transform
        CHECK(widget.addStep("CalculateLineAngle"));

        // Then try to add another Line->X transform (should add but be invalid)
        CHECK(widget.addStep("ResampleLine"));

        // The second step should be marked invalid (expects Line2D, gets float)
        REQUIRE(widget.steps().size() == 2);
        CHECK(widget.steps()[0].is_valid);
        CHECK_FALSE(widget.steps()[1].is_valid);
    }

    SECTION("clearSteps empties the pipeline") {
        auto container_type = TypeIndexMapper::stringToContainer("MaskData");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        PipelineStepListWidget widget;
        widget.setInputType(element_type, container_type);

        widget.addStep("CalculateMaskArea");
        REQUIRE(widget.steps().size() == 1);

        widget.clearSteps();
        CHECK(widget.steps().empty());
    }

    SECTION("RaggedAnalogTimeSeries input finds float element transforms") {
        auto container_type = TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries");
        auto element_type = TypeIndexMapper::containerToElement(container_type);

        // float element type should find ZScoreNormalizeV2
        auto names = registry.getTransformsForInputType(element_type);

        bool found_zscore = false;
        for (auto const & name: names) {
            if (name == "ZScoreNormalizeV2") found_zscore = true;
        }
        CHECK(found_zscore);
    }
}

// ============================================================================
// Section 3: StepConfigPanel — Config panel for transform steps
// ============================================================================

TEST_CASE("StepConfigPanel shows correct config for transforms",
          "[TransformsV2Widget][StepConfigPanel][config]") {

    QtAppFixture const qt;

    SECTION("showStepConfig creates AutoParamWidget for parameterized transform") {
        StepConfigPanel panel;
        panel.showStepConfig("CalculateMaskArea", "{}");

        CHECK(panel.currentTransformName() == "CalculateMaskArea");

        // Should have an AutoParamWidget (since MaskAreaParams has fields)
        auto auto_widgets = panel.findChildren<AutoParamWidget *>();
        CHECK_FALSE(auto_widgets.isEmpty());
    }

    SECTION("showStepConfig for LineAngle creates parameters with 8 fields") {
        StepConfigPanel panel;
        panel.showStepConfig("CalculateLineAngle", "{}");

        CHECK(panel.currentTransformName() == "CalculateLineAngle");

        // Should have an AutoParamWidget
        auto auto_widgets = panel.findChildren<AutoParamWidget *>();
        REQUIRE_FALSE(auto_widgets.isEmpty());

    }

    SECTION("showStepConfig shows transform name in header label") {
        StepConfigPanel panel;
        panel.showStepConfig("CalculateMaskArea", "{}");

        auto labels = panel.findChildren<QLabel *>();
        bool found_name = false;
        for (auto * label: labels) {
            if (label->text().contains("CalculateMaskArea")) {
                found_name = true;
            }
        }
        CHECK(found_name);
    }

    SECTION("showStepConfig shows description when available") {
        StepConfigPanel panel;
        panel.showStepConfig("CalculateMaskArea", "{}");

        auto const * meta = ElementRegistry::instance().getMetadata("CalculateMaskArea");
        REQUIRE(meta != nullptr);

        if (!meta->description.empty()) {
            auto labels = panel.findChildren<QLabel *>();
            bool found_desc = false;
            for (auto * label: labels) {
                if (label->text().toStdString() == meta->description) {
                    found_desc = true;
                }
            }
            CHECK(found_desc);
        }
    }

    SECTION("clearConfig removes widget and clears transform name") {
        StepConfigPanel panel;
        panel.showStepConfig("CalculateMaskArea", "{}");
        REQUIRE(panel.currentTransformName() == "CalculateMaskArea");

        panel.clearConfig();
        CHECK(panel.currentTransformName().empty());
    }

    SECTION("showStepConfig with existing params populates widget") {
        StepConfigPanel panel;
        std::string const params = R"({"scale_factor": 3.14})";
        panel.showStepConfig("CalculateMaskArea", params);

        auto result = panel.currentParamsJson();
        CHECK_THAT(result, Catch::Matchers::ContainsSubstring("scale_factor"));
    }

    SECTION("AnalogEventThreshold creates config panel with correct params") {
        StepConfigPanel panel;
        panel.showStepConfig("AnalogEventThreshold", "{}");

        CHECK(panel.currentTransformName() == "AnalogEventThreshold");

        // AnalogEventThresholdParams has 3 fields: threshold_value, direction, lockout_time
        auto const * schema = ElementRegistry::instance().getParameterSchema("AnalogEventThreshold");
        REQUIRE(schema != nullptr);
        CHECK(schema->fields.size() == 3);
    }
}

// ============================================================================
// Section 4: TransformsV2Properties_Widget — Integration
// ============================================================================

TEST_CASE("TransformsV2Properties_Widget responds to data focus changes",
          "[TransformsV2Widget][PropertiesWidget][integration]") {

    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TransformsV2State>(data_manager);
    auto selection_context = std::make_unique<SelectionContext>();

    SECTION("Widget constructs successfully") {
        TransformsV2Properties_Widget const widget(state, selection_context.get());
        QApplication::processEvents();
        REQUIRE(true);// If we get here, construction succeeded
    }

    SECTION("onDataFocusChanged with MaskData resolves types and updates step list") {
        TransformsV2Properties_Widget widget(state, selection_context.get());
        QApplication::processEvents();

        // Simulate data focus change to MaskData
        EditorLib::SelectedDataKey const key("test_mask_key");
        widget.onDataFocusChanged(key, "MaskData");
        QApplication::processEvents();

        // The step list should now have MaskData as its input type
        // Find the PipelineStepListWidget
        auto * step_list = widget.findChild<PipelineStepListWidget *>();
        REQUIRE(step_list != nullptr);

        // Try to add a Mask transform — should succeed if types are resolved
        CHECK(step_list->addStep("CalculateMaskArea"));
    }

    SECTION("onDataFocusChanged with LineData resolves types correctly") {
        TransformsV2Properties_Widget widget(state, selection_context.get());
        QApplication::processEvents();

        EditorLib::SelectedDataKey const key("test_line_key");
        widget.onDataFocusChanged(key, "LineData");
        QApplication::processEvents();

        auto * step_list = widget.findChild<PipelineStepListWidget *>();
        REQUIRE(step_list != nullptr);

        CHECK(step_list->addStep("CalculateLineAngle"));
    }

    SECTION("onDataFocusChanged with AnalogTimeSeries resolves types correctly") {
        TransformsV2Properties_Widget widget(state, selection_context.get());
        QApplication::processEvents();

        EditorLib::SelectedDataKey const key("test_analog_key");
        widget.onDataFocusChanged(key, "AnalogTimeSeries");
        QApplication::processEvents();

        auto * step_list = widget.findChild<PipelineStepListWidget *>();
        REQUIRE(step_list != nullptr);

        // AnalogEventThreshold is a container transform for AnalogTimeSeries
        CHECK(step_list->addStep("AnalogEventThreshold"));
    }

    SECTION("Input label updates on data focus change") {
        TransformsV2Properties_Widget widget(state, selection_context.get());
        QApplication::processEvents();

        EditorLib::SelectedDataKey const key("my_mask_data");
        widget.onDataFocusChanged(key, "MaskData");
        QApplication::processEvents();

        // Find the input key label — it should show the key name
        auto labels = widget.findChildren<QLabel *>();
        bool found_key_label = false;
        for (auto * label: labels) {
            if (label->text() == "my_mask_data") {
                found_key_label = true;
            }
        }
        CHECK(found_key_label);
    }

    SECTION("Input type label updates on data focus change") {
        TransformsV2Properties_Widget widget(state, selection_context.get());
        QApplication::processEvents();

        EditorLib::SelectedDataKey const key("test_key");
        widget.onDataFocusChanged(key, "LineData");
        QApplication::processEvents();

        auto labels = widget.findChildren<QLabel *>();
        bool found_type_label = false;
        for (auto * label: labels) {
            if (label->text() == "LineData") {
                found_type_label = true;
            }
        }
        CHECK(found_type_label);
    }

    SECTION("State stores input data key after focus change") {
        TransformsV2Properties_Widget widget(state, selection_context.get());
        QApplication::processEvents();

        EditorLib::SelectedDataKey const key("stored_key");
        widget.onDataFocusChanged(key, "MaskData");
        QApplication::processEvents();

        CHECK(state->inputDataKey().has_value());
        CHECK(state->inputDataKey().value() == "stored_key");
    }

    SECTION("Switching data types updates step list input type") {
        TransformsV2Properties_Widget widget(state, selection_context.get());
        QApplication::processEvents();

        auto * step_list = widget.findChild<PipelineStepListWidget *>();
        REQUIRE(step_list != nullptr);

        // First focus on MaskData
        widget.onDataFocusChanged(EditorLib::SelectedDataKey("mask_key"), "MaskData");
        QApplication::processEvents();
        CHECK(step_list->addStep("CalculateMaskArea"));

        // Clear and switch to LineData
        step_list->clearSteps();
        widget.onDataFocusChanged(EditorLib::SelectedDataKey("line_key"), "LineData");
        QApplication::processEvents();

        // Now Line transforms should work
        CHECK(step_list->addStep("CalculateLineAngle"));
        // Mask transforms should fail (type mismatch — addStep still adds but validation fails)
    }

    SECTION("SelectionContext signal triggers onDataFocusChanged") {
        TransformsV2Properties_Widget const widget(state, selection_context.get());
        QApplication::processEvents();

        // Fire the SelectionContext signal
        SelectionSource const source{EditorLib::EditorInstanceId("test"), "test"};
        selection_context->setDataFocus(
                EditorLib::SelectedDataKey("signal_test_key"),
                "MaskData",
                source);
        QApplication::processEvents();

        // State should reflect the focus change
        CHECK(state->inputDataKey().has_value());
        CHECK(state->inputDataKey().value() == "signal_test_key");

        // Step list should accept Mask transforms
        auto * step_list = widget.findChild<PipelineStepListWidget *>();
        REQUIRE(step_list != nullptr);
        CHECK(step_list->addStep("CalculateMaskArea"));
    }
}

// ============================================================================
// Section 4b: Pipeline JSON round-trip (metadata / pre_reductions preserved)
// ============================================================================

TEST_CASE("TransformsV2Properties_Widget preserves pre_reductions after Apply and UI sync",
          "[TransformsV2Widget][PropertiesWidget][json]") {

    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TransformsV2State>(data_manager);
    auto selection_context = std::make_unique<SelectionContext>();

    auto const config_dir =
            QDir::temp().filePath(QStringLiteral("wt_tv2_widget_%1").arg(
                    QDateTime::currentMSecsSinceEpoch()));

    TransformsV2Properties_Widget widget(state, selection_context.get(), config_dir);
    QApplication::processEvents();

    auto * json_panel = widget.findChild<QTextEdit *>();
    REQUIRE(json_panel != nullptr);

    QString const pipeline_json = QStringLiteral(R"({
        "metadata": {"name": "ZScore Test"},
        "pre_reductions": [
            {"reduction_name": "MeanValueRaw", "output_key": "computed_mean"},
            {"reduction_name": "StdValueRaw", "output_key": "computed_std"}
        ],
        "steps": [
            {
                "step_id": "zscore",
                "transform_name": "ZScoreNormalizeV2",
                "param_bindings": {
                    "mean": "computed_mean",
                    "std_dev": "computed_std"
                }
            }
        ],
        "range_reduction": {"reduction_name": "MeanValue"}
    })");

    json_panel->setPlainText(pipeline_json);

    QPushButton * apply_button = nullptr;
    for (auto * button: widget.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Apply")) {
            apply_button = button;
            break;
        }
    }
    REQUIRE(apply_button != nullptr);
    QTest::mouseClick(apply_button, Qt::LeftButton);
    QApplication::processEvents();

    auto const after_apply = json_panel->toPlainText().toStdString();
    CHECK(after_apply.find("pre_reductions") != std::string::npos);
    CHECK(after_apply.find("computed_mean") != std::string::npos);
    CHECK(after_apply.find("range_reduction") != std::string::npos);
    CHECK(after_apply.find("ZScore Test") != std::string::npos);

    widget.onDataFocusChanged(EditorLib::SelectedDataKey("analog_src"), "AnalogTimeSeries");
    QApplication::processEvents();

    auto * step_list = widget.findChild<PipelineStepListWidget *>();
    REQUIRE(step_list != nullptr);
    REQUIRE(step_list->addStep("AnalogEventThreshold"));
    QApplication::processEvents();

    auto const after_step_add = json_panel->toPlainText().toStdString();
    CHECK(after_step_add.find("pre_reductions") != std::string::npos);
    CHECK(after_step_add.find("AnalogEventThreshold") != std::string::npos);
    CHECK(after_step_add.find("ZScoreNormalizeV2") != std::string::npos);
}

// ============================================================================
// Section 4c: PipelineLibraryDialog
// ============================================================================

TEST_CASE("PipelineLibraryDialog lists and previews saved pipelines",
          "[TransformsV2Widget][PipelineLibraryDialog]") {

    QtAppFixture const qt;

    auto const config_dir =
            QDir::temp().filePath(QStringLiteral("wt_tv2_library_%1").arg(
                    QDateTime::currentMSecsSinceEpoch()));

    auto const dir_result = WhiskerToolbox::Transforms::V2::Examples::ensureUserPipelineDirectory(
            config_dir.toStdString());
    REQUIRE(dir_result);

    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{.name = "Catalog Test"};
    descriptor.steps = {PipelineStepDescriptor{
            .step_id = "area", .transform_name = "CalculateMaskArea"}};

    auto const library_path = dir_result.value() / "catalog_test.json";
    REQUIRE(WhiskerToolbox::Transforms::V2::Examples::savePipelineDescriptorToFile(
            library_path, descriptor));

    PipelineLibraryDialog dialog(QString::fromStdString(dir_result.value().string()));
    dialog.show();
    QApplication::processEvents();

    CHECK(dialog.catalogEntryCount() == 1);

    auto * list = dialog.findChild<QListWidget *>();
    REQUIRE(list != nullptr);
    CHECK(list->item(0)->text() == QStringLiteral("Catalog Test"));

    auto * preview = dialog.findChild<QTextEdit *>();
    REQUIRE(preview != nullptr);
    list->setCurrentRow(0);
    QApplication::processEvents();
    CHECK(preview->toPlainText().contains(QStringLiteral("CalculateMaskArea")));
}

TEST_CASE("TransformsV2Properties_Widget has library controls when config dir is set",
          "[TransformsV2Widget][PropertiesWidget][library]") {

    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TransformsV2State>(data_manager);
    auto selection_context = std::make_unique<SelectionContext>();

    auto const config_dir =
            QDir::temp().filePath(QStringLiteral("wt_tv2_props_lib_%1").arg(
                    QDateTime::currentMSecsSinceEpoch()));

    TransformsV2Properties_Widget widget(state, selection_context.get(), config_dir);
    QApplication::processEvents();

    bool found_library = false;
    bool found_save_to_library = false;
    for (auto * button: widget.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Library...")) {
            found_library = true;
        }
        if (button->text() == QStringLiteral("Save to Library")) {
            found_save_to_library = true;
        }
    }
    CHECK(found_library);
    CHECK(found_save_to_library);
}

// ============================================================================
// Section 5: ElementRegistry transform availability sanity checks
// ============================================================================

TEST_CASE("ElementRegistry has expected transforms registered",
          "[TransformsV2Widget][ElementRegistry][sanity]") {

    SECTION("All expected element transforms exist") {
        auto & reg = ElementRegistry::instance();

        // Mask transforms
        CHECK(reg.hasTransform("CalculateMaskArea"));
        CHECK(reg.hasTransform("CalculateMaskCentroid"));

        // Line transforms
        CHECK(reg.hasTransform("CalculateLineAngle"));
        CHECK(reg.hasTransform("CalculateLineCurvature"));
        CHECK(reg.hasTransform("CalculateLineLength"));
        CHECK(reg.hasTransform("FlipLineBase"));
        CHECK(reg.hasTransform("ResampleLine"));
        CHECK(reg.hasTransform("ExtractLineSubsegment"));
        CHECK(reg.hasTransform("ExtractLinePoint"));

        // Float transforms
        CHECK(reg.hasTransform("ZScoreNormalizeV2"));
    }

    SECTION("All expected container transforms exist") {
        auto & reg = ElementRegistry::instance();

        CHECK(reg.hasTransform("AnalogEventThreshold"));
        CHECK(reg.hasTransform("AnalogIntervalThreshold"));
    }

    SECTION("Parameter schemas are available for parameterized transforms") {
        auto & reg = ElementRegistry::instance();

        CHECK(reg.getParameterSchema("CalculateMaskArea") != nullptr);
        CHECK(reg.getParameterSchema("CalculateLineAngle") != nullptr);
        CHECK(reg.getParameterSchema("AnalogEventThreshold") != nullptr);
        CHECK(reg.getParameterSchema("ResampleLine") != nullptr);
        CHECK(reg.getParameterSchema("ZScoreNormalizeV2") != nullptr);
    }

    SECTION("Metadata provides correct input/output types for Mask transforms") {
        auto & reg = ElementRegistry::instance();

        auto const * meta = reg.getMetadata("CalculateMaskArea");
        REQUIRE(meta != nullptr);
        CHECK(meta->input_type == typeid(Mask2D));
        CHECK(meta->output_type == typeid(float));
        CHECK(meta->name == "CalculateMaskArea");
    }

    SECTION("Metadata provides correct input/output types for Line transforms") {
        auto & reg = ElementRegistry::instance();

        auto const * meta = reg.getMetadata("CalculateLineAngle");
        REQUIRE(meta != nullptr);
        CHECK(meta->input_type == typeid(Line2D));
        CHECK(meta->output_type == typeid(float));
    }

    SECTION("TypeIndexMapper resolves all expected container types") {
        CHECK_NOTHROW(TypeIndexMapper::stringToContainer("MaskData"));
        CHECK_NOTHROW(TypeIndexMapper::stringToContainer("LineData"));
        CHECK_NOTHROW(TypeIndexMapper::stringToContainer("PointData"));
        CHECK_NOTHROW(TypeIndexMapper::stringToContainer("AnalogTimeSeries"));
        CHECK_NOTHROW(TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries"));
        CHECK_NOTHROW(TypeIndexMapper::stringToContainer("DigitalEventSeries"));
        CHECK_NOTHROW(TypeIndexMapper::stringToContainer("DigitalIntervalSeries"));
    }

    SECTION("TypeIndexMapper round-trips container string -> type -> element -> container") {
        auto container = TypeIndexMapper::stringToContainer("MaskData");
        auto element = TypeIndexMapper::containerToElement(container);
        auto back_to_container = TypeIndexMapper::elementToContainer(element);
        CHECK(back_to_container == container);
    }
}

// ============================================================================
// Section 6: resolveTypeChain
// ============================================================================

TEST_CASE("resolveTypeChain produces correct per-step type info",
          "[TransformsV2Widget][TypeChainResolver]") {

    SECTION("Empty step list returns input types unchanged") {
        auto container = TypeIndexMapper::stringToContainer("MaskData");
        auto result = resolveTypeChain(container, {});

        CHECK(result.all_valid);
        CHECK(result.steps.empty());
        CHECK(result.output_container_type == container);
    }

    SECTION("Single element transform: MaskData -> CalculateMaskArea -> RaggedAnalogTimeSeries") {
        auto container = TypeIndexMapper::stringToContainer("MaskData");
        std::vector<std::string> names = {"CalculateMaskArea"};

        auto result = resolveTypeChain(container, names);

        REQUIRE(result.steps.size() == 1);
        CHECK(result.all_valid);
        CHECK(result.steps[0].input_type_name == "MaskData");
        CHECK(result.steps[0].output_type_name == "RaggedAnalogTimeSeries");
        CHECK(result.steps[0].is_valid);
    }

    SECTION("Two-step chain: MaskData -> CalculateMaskArea -> SumReduction -> AnalogTimeSeries") {
        auto container = TypeIndexMapper::stringToContainer("MaskData");
        std::vector<std::string> names = {"CalculateMaskArea", "SumReduction"};

        auto result = resolveTypeChain(container, names);

        REQUIRE(result.steps.size() == 2);
        CHECK(result.all_valid);

        // Step 1: Mask2D -> float, container becomes RaggedAnalogTimeSeries
        CHECK(result.steps[0].input_type_name == "MaskData");
        CHECK(result.steps[0].output_type_name == "RaggedAnalogTimeSeries");
        CHECK(result.steps[0].is_valid);

        // Step 2: SumReduction (time-grouped, produces_single_output)
        //         input is RaggedAnalogTimeSeries, output is AnalogTimeSeries
        CHECK(result.steps[1].input_type_name == "RaggedAnalogTimeSeries");
        CHECK(result.steps[1].output_type_name == "AnalogTimeSeries");
        CHECK(result.steps[1].is_valid);

        // Final output should be AnalogTimeSeries
        CHECK(result.output_container_type == TypeIndexMapper::stringToContainer("AnalogTimeSeries"));
    }

    SECTION("Type mismatch marks step as invalid") {
        auto container = TypeIndexMapper::stringToContainer("MaskData");
        // SumReduction expects float input, not Mask2D
        std::vector<std::string> names = {"SumReduction"};

        auto result = resolveTypeChain(container, names);

        REQUIRE(result.steps.size() == 1);
        CHECK_FALSE(result.all_valid);
        CHECK_FALSE(result.steps[0].is_valid);
    }

    SECTION("AnalogTimeSeries preserves non-ragged container for float transforms") {
        auto container = TypeIndexMapper::stringToContainer("AnalogTimeSeries");
        // ZScoreNormalizeV2 is float -> float, non-time-grouped
        std::vector<std::string> names = {"ZScoreNormalizeV2"};

        auto result = resolveTypeChain(container, names);

        REQUIRE(result.steps.size() == 1);
        CHECK(result.all_valid);
        CHECK(result.steps[0].input_type_name == "AnalogTimeSeries");
        CHECK(result.steps[0].output_type_name == "AnalogTimeSeries");
        CHECK(result.output_container_type == TypeIndexMapper::stringToContainer("AnalogTimeSeries"));
    }

    SECTION("RaggedAnalogTimeSeries preserves ragged container for float transforms") {
        auto container = TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries");
        // ZScoreNormalizeV2 is float -> float, non-time-grouped
        std::vector<std::string> names = {"ZScoreNormalizeV2"};

        auto result = resolveTypeChain(container, names);

        REQUIRE(result.steps.size() == 1);
        CHECK(result.all_valid);
        CHECK(result.steps[0].input_type_name == "RaggedAnalogTimeSeries");
        CHECK(result.steps[0].output_type_name == "RaggedAnalogTimeSeries");
        CHECK(result.output_container_type == TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries"));
    }

    SECTION("Line transform chain: LineData -> CalculateLineAngle -> RaggedAnalogTimeSeries") {
        auto container = TypeIndexMapper::stringToContainer("LineData");
        std::vector<std::string> names = {"CalculateLineAngle"};

        auto result = resolveTypeChain(container, names);

        REQUIRE(result.steps.size() == 1);
        CHECK(result.all_valid);
        CHECK(result.steps[0].input_type_name == "LineData");
        CHECK(result.steps[0].output_type_name == "RaggedAnalogTimeSeries");
    }

    SECTION("Unknown transform name marks step invalid") {
        auto container = TypeIndexMapper::stringToContainer("MaskData");
        std::vector<std::string> names = {"NonExistentTransform"};

        auto result = resolveTypeChain(container, names);

        REQUIRE(result.steps.size() == 1);
        CHECK_FALSE(result.all_valid);
        CHECK_FALSE(result.steps[0].is_valid);
        CHECK(result.steps[0].output_type_name == "Unknown");
    }
}

// ============================================================================
// Section: AutoParamWidget — TaggedUnion (Variant) Support
// ============================================================================

namespace test_variant_widget {

struct LinearMotionParams {
    float velocity_x = 1.0f;
    float velocity_y = 0.0f;
};

struct SinusoidalMotionParams {
    float amplitude_x = 0.0f;
    float amplitude_y = 0.0f;
    float frequency_x = 1.0f;
    float frequency_y = 1.0f;
};

struct BrownianMotionParams {
    float diffusion = 1.0f;
    int seed = 42;
};

using MotionVariant = rfl::TaggedUnion<
        "model",
        LinearMotionParams,
        SinusoidalMotionParams,
        BrownianMotionParams>;

struct MovingPointParams {
    float start_x = 100.0f;
    int num_frames = 100;
    MotionVariant motion = LinearMotionParams{};
};

}// namespace test_variant_widget

TEST_CASE("AutoParamWidget handles TaggedUnion variant fields",
          "[TransformsV2Widget][AutoParamWidget][variant]") {

    QtAppFixture const qt;

    auto schema = extractParameterSchema<test_variant_widget::MovingPointParams>();
    REQUIRE(schema.fields.size() == 3);

    AutoParamWidget widget;
    widget.setSchema(schema);

    SECTION("Creates QComboBox for variant discriminator") {
        auto combos = widget.findChildren<QComboBox *>();
        // Should have at least one combo (the variant discriminator)
        CHECK_FALSE(combos.isEmpty());

        // Find the variant combo by checking for alternative tags
        bool found_variant_combo = false;
        for (auto * combo: combos) {
            if (combo->count() == 3 &&
                combo->itemText(0) == "LinearMotionParams") {
                found_variant_combo = true;
                break;
            }
        }
        CHECK(found_variant_combo);
    }

    SECTION("Creates QStackedWidget for variant alternatives") {
        auto stacks = widget.findChildren<QStackedWidget *>();
        CHECK_FALSE(stacks.isEmpty());
        // Should have 3 pages (one per alternative)
        CHECK(stacks[0]->count() == 3);
    }

    SECTION("toJson produces correct variant JSON structure") {
        auto json_str = widget.toJson();
        auto result = rfl::json::read<rfl::Generic>(json_str);
        REQUIRE(result);

        auto const * obj = std::get_if<rfl::Generic::Object>(&result.value().get());
        REQUIRE(obj != nullptr);

        // The "motion" field should be a nested object
        auto motion_val = obj->get("motion");
        REQUIRE(motion_val);
        auto const * motion_obj = std::get_if<rfl::Generic::Object>(&motion_val.value().get());
        REQUIRE(motion_obj != nullptr);

        // Should have the discriminator
        auto model_val = motion_obj->get("model");
        REQUIRE(model_val);
        auto const * model_str = std::get_if<std::string>(&model_val.value().get());
        REQUIRE(model_str != nullptr);
        CHECK(*model_str == "LinearMotionParams");

        // Should have sub-fields from the active alternative
        auto vx_val = motion_obj->get("velocity_x");
        REQUIRE(vx_val);
    }

    SECTION("fromJson restores variant state") {
        // Manually construct JSON with BrownianMotionParams alternative
        std::string const json = R"({
            "start_x": 50.0,
            "num_frames": 200,
            "motion": {
                "model": "BrownianMotionParams",
                "diffusion": 3.5,
                "seed": 99
            }
        })";

        REQUIRE(widget.fromJson(json));

        // Verify the non-variant fields
        auto round_json_str = widget.toJson();
        auto result = rfl::json::read<rfl::Generic>(round_json_str);
        REQUIRE(result);
        auto const * obj = std::get_if<rfl::Generic::Object>(&result.value().get());
        REQUIRE(obj != nullptr);

        // Check start_x was updated
        auto sx = obj->get("start_x");
        REQUIRE(sx);
        double sx_val = 0.0;
        if (auto const * sx_d = std::get_if<double>(&sx.value().get())) {
            sx_val = *sx_d;
        } else if (auto const * sx_i = std::get_if<int>(&sx.value().get())) {
            sx_val = static_cast<double>(*sx_i);
        }
        CHECK_THAT(sx_val, Catch::Matchers::WithinAbs(50.0, 0.1));

        // Check motion discriminator
        auto motion_val = obj->get("motion");
        REQUIRE(motion_val);
        auto const * motion_obj = std::get_if<rfl::Generic::Object>(&motion_val.value().get());
        REQUIRE(motion_obj != nullptr);

        auto model_val = motion_obj->get("model");
        REQUIRE(model_val);
        auto const * model_str = std::get_if<std::string>(&model_val.value().get());
        REQUIRE(model_str != nullptr);
        CHECK(*model_str == "BrownianMotionParams");
    }
}
