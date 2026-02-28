/**
 * @file WidgetCorpus.cpp
 * @brief Implementation of graduated real widget type registration for fuzz testing
 *
 * Phase 6 of the Workspace Fuzz Testing Roadmap.
 */

#include "WidgetCorpus.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/ZoneTypes.hpp"

// Real widget state headers (Minimal level)
#include "Test_Widget/TestWidgetState.hpp"

// Real widget state headers (Core level)
#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataImport_Widget/DataImportWidgetState.hpp"
#include "DataTransform_Widget/DataTransformWidgetState.hpp"

// Real widget state headers (Visualization level)
#include "DataViewer_Widget/Core/DataViewerState.hpp"

// Real widget state headers (Media level)
#include "Media_Widget/Core/MediaWidgetState.hpp"

#include "DataManager/DataManager.hpp"

#include <QWidget>

// ============================================================================
// Corpus entries at each level
// ============================================================================

static std::vector<CorpusEntry> const kMinimalEntries = {
    {.type_id = "TestWidget", .allow_multiple = false},
};

static std::vector<CorpusEntry> const kCoreEntries = {
    {.type_id = "DataInspector", .allow_multiple = true},
    {.type_id = "DataImportWidget", .allow_multiple = false},
    {.type_id = "DataTransformWidget", .allow_multiple = false},
};

static std::vector<CorpusEntry> const kVisualizationEntries = {
    {.type_id = "DataViewerWidget", .allow_multiple = true},
};

static std::vector<CorpusEntry> const kMediaEntries = {
    {.type_id = "MediaWidget", .allow_multiple = true},
};

std::vector<CorpusEntry> corpusEntries(CorpusLevel level) {
    std::vector<CorpusEntry> entries;

    switch (level) {
    case CorpusLevel::Media:
        entries.insert(entries.end(), kMediaEntries.begin(), kMediaEntries.end());
        [[fallthrough]];
    case CorpusLevel::Visualization:
        entries.insert(entries.end(), kVisualizationEntries.begin(), kVisualizationEntries.end());
        [[fallthrough]];
    case CorpusLevel::Core:
        entries.insert(entries.end(), kCoreEntries.begin(), kCoreEntries.end());
        [[fallthrough]];
    case CorpusLevel::Minimal:
        entries.insert(entries.end(), kMinimalEntries.begin(), kMinimalEntries.end());
        [[fallthrough]];
    case CorpusLevel::Mock:
        // Mock types are registered separately via registerMockTypes()
        break;
    }

    return entries;
}

// ============================================================================
// Registration
// ============================================================================

void registerCorpusTypes(
    EditorRegistry * registry,
    CorpusLevel level,
    std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        return;
    }

    // Minimal view/properties factories — just enough for ADS dock wrapping.
    // We don't instantiate the real View/Properties UI classes to avoid
    // pulling in QGraphicsView, QColorDialog, etc. dependencies.
    auto make_view = [](std::shared_ptr<EditorState> /*state*/) -> QWidget * {
        return new QWidget();
    };
    auto make_props = [](std::shared_ptr<EditorState> /*state*/) -> QWidget * {
        return new QWidget();
    };

    auto const entries = corpusEntries(level);

    for (auto const & entry : entries) {
        if (entry.type_id == "TestWidget") {
            auto dm = data_manager;
            registry->registerType({
                .type_id = "TestWidget",
                .display_name = "Test Widget",
                .preferred_zone = Zone::Center,
                .properties_zone = Zone::Right,
                .allow_multiple = false,
                .create_state = [dm]() {
                    return std::make_shared<TestWidgetState>(dm);
                },
                .create_view = make_view,
                .create_properties = make_props,
            });
        } else if (entry.type_id == "DataInspector") {
            registry->registerType({
                .type_id = "DataInspector",
                .display_name = "Data Inspector",
                .preferred_zone = Zone::Center,
                .properties_zone = Zone::Right,
                .allow_multiple = true,
                .create_state = []() {
                    return std::make_shared<DataInspectorState>();
                },
                .create_view = make_view,
                .create_properties = make_props,
            });
        } else if (entry.type_id == "DataImportWidget") {
            registry->registerType({
                .type_id = "DataImportWidget",
                .display_name = "Data Import",
                .preferred_zone = Zone::Right,
                .properties_zone = Zone::Right,
                .allow_multiple = false,
                .create_state = []() {
                    return std::make_shared<DataImportWidgetState>();
                },
                .create_view = make_view,
                .create_properties = make_props,
            });
        } else if (entry.type_id == "DataTransformWidget") {
            registry->registerType({
                .type_id = "DataTransformWidget",
                .display_name = "Data Transforms",
                .preferred_zone = Zone::Right,
                .properties_zone = Zone::Right,
                .allow_multiple = false,
                .create_state = []() {
                    return std::make_shared<DataTransformWidgetState>();
                },
                .create_view = make_view,
                .create_properties = make_props,
            });
        } else if (entry.type_id == "DataViewerWidget") {
            registry->registerType({
                .type_id = "DataViewerWidget",
                .display_name = "Data Viewer",
                .preferred_zone = Zone::Center,
                .properties_zone = Zone::Right,
                .allow_multiple = true,
                .create_state = []() {
                    return std::make_shared<DataViewerState>();
                },
                .create_view = make_view,
                .create_properties = make_props,
            });
        } else if (entry.type_id == "MediaWidget") {
            registry->registerType({
                .type_id = "MediaWidget",
                .display_name = "Media Viewer",
                .preferred_zone = Zone::Center,
                .properties_zone = Zone::Right,
                .allow_multiple = true,
                .create_state = []() {
                    return std::make_shared<MediaWidgetState>();
                },
                .create_view = make_view,
                .create_properties = make_props,
            });
        }
    }
}
