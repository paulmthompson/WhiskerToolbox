/**
 * @file DataSynthesizerWidgetRegistration.cpp
 * @brief Registration implementation for the DataSynthesizer editor type
 */

#include "DataSynthesizerWidgetRegistration.hpp"

#include "Core/DataSynthesizerState.hpp"
#include "UI/DataSynthesizerProperties_Widget.hpp"
#include "UI/DataSynthesizerView_Widget.hpp"

#include "EditorState/EditorRegistry.hpp"

#include <iostream>

namespace DataSynthesizerWidgetModule {

void registerTypes(EditorRegistry * registry, std::shared_ptr<DataManager> const & data_manager) {
    if (!registry) {
        std::cerr << "DataSynthesizerWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    if (!data_manager) {
        std::cerr << "DataSynthesizerWidgetModule::registerTypes: data_manager is null" << std::endl;
        return;
    }

    auto const & dm = data_manager;

    registry->registerType({.type_id = QStringLiteral("DataSynthesizerWidget"),
                            .display_name = QStringLiteral("Data Synthesizer"),
                            .icon_path = QString{},
                            .menu_path = QStringLiteral("View/Tools"),

                            .preferred_zone = Zone::Right,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,

                            .allow_multiple = false,

                            .create_state = nullptr,
                            .create_view = nullptr,
                            .create_properties = nullptr,

                            .create_editor_custom = [dm](EditorRegistry * reg) -> EditorRegistry::EditorInstance {
                                auto state = std::make_shared<DataSynthesizerState>();

                                auto * view = new DataSynthesizerView_Widget(state, dm);
                                auto * props = new DataSynthesizerProperties_Widget(state, dm);

                                // Connect preview: properties "Preview" button → view OpenGL widget
                                QObject::connect(props, &DataSynthesizerProperties_Widget::previewRequested,
                                                 view, &DataSynthesizerView_Widget::setPreviewData);

                                reg->registerState(state);

                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = view,
                                        .properties = props};
                            }});
}

}// namespace DataSynthesizerWidgetModule
